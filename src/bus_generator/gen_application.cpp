#include "gen_application.hpp"

#include "common/shared_defs.hpp"
#include "common/console_tables.hpp"

#include "dpdk_cpp/dpdk_port_class.hpp"
#include "dpdk_cpp/dpdk_poolsetter_class.hpp"
#include "dpdk_cpp/dpdk_mempool_class.hpp"
#include "dpdk_cpp/dpdk_info_class.hpp"

#include "goose_traffic_gen.hpp"
#include "sv_traffic_gen.hpp"

#include "cxxopts.hpp"

const unsigned BURST_SIZE = 32;

template< typename TxUnitArray >
static void display_tx_units_info(TxUnitArray &txUnits)
{
    size_t MaxNum = 0, MinNum = 0, TotalNum = 0;
    for (size_t i=0;i<txUnits.size();++i) {
        for (const auto &blk : txUnits[i].blocks) {
            size_t size = blk.packets.size();

            if (size > MaxNum) {
                MaxNum = size;
            }
            if (size < MinNum || MinNum == 0) {
                MinNum = size;
            }
            TotalNum += size;
        }
    }

    std::cout << "TxUnits: \n"
              << "\tTotal blocks:  " << txUnits.size() << "\n"
              << "\tTotal packets: " << TotalNum << "\n"
              << "\tMax in block:  " << MaxNum << "\n"
              << "\tMin in block:  " << MinNum
              << std::endl;
}

struct TxCycleConfig
{
    rte_mempool* pool = nullptr;
    uint16_t     nicPortID = 0;
    uint16_t     nicQueueID = 0;
};

template<
    typename GenClass,
    size_t (GenClass::*Amend)(uint8_t *packet, const typename GenClass::Desc &desc)
        = &GenClass::AmendPacket
>
static void tx_packets_cycle(TxCycleConfig &conf, GenAppStat &stat, GenClass &gen, StopVarType &doWork)
{
    typename GenClass::TxUnitArray &txUnits = gen.GetTxUnits();
    if (txUnits.empty()) {
        throw std::runtime_error("TX unit array is empty! Nothing to send!");
    }
    /* display_tx_units_info< typename GenClass::TxUnitArray >(txUnits); */

    std::cout << "\n\tStart main loop\n";
    /* set_thread_priority(DEF_GENERATOR_PRIORITY); */

    // Main cycle
    unsigned txUnitIdx = 0;
    rte_mbuf* mbufs[BURST_SIZE] = { 0 };
    stat.procStat.MarkStartCycling();
    uint64_t secStartTick = DPDK::Clocks::get_current_ticks();
    while (doWork) {
        stat.procStat.MarkProcBegin();
        for (const auto &blk : txUnits[txUnitIdx].blocks) {
            unsigned count = blk.packets.size(), sendNum = 0;
            while (sendNum < count) {
                unsigned num = ((count - sendNum) >= BURST_SIZE) ? BURST_SIZE
                                                                 : (count - sendNum);
                if (rte_pktmbuf_alloc_bulk(conf.pool, mbufs, num) == 0) {
                    for (size_t i=0;i<num;++i) {
                        uint8_t *packet = rte_pktmbuf_mtod(mbufs[i], uint8_t *);

                        mbufs[i]->pkt_len = (gen.*Amend)(packet, blk.packets[sendNum + i]);
                        mbufs[i]->data_len = mbufs[i]->pkt_len;
                    }

                    uint16_t nb_tx = rte_eth_tx_burst(conf.nicPortID, conf.nicQueueID, mbufs, num);
                    if (nb_tx < num) {
                        for (uint16_t i=nb_tx;i<num;i++) {
                            rte_pktmbuf_free(mbufs[i]);
                        }
                        stat.errSendCnt += num - nb_tx;
                    }
                } else {
                    rte_eth_tx_done_cleanup(conf.nicPortID, conf.nicQueueID, 0);
                    continue;
                }

                sendNum += num;
            }
        }
        stat.procStat.MarkProcEnd();

        txUnitIdx = (txUnitIdx + 1) % txUnits.size();
        if (txUnitIdx == 0) {
            // New PPS(new second pulse)
            secStartTick = DPDK::Clocks::delay_until_ticks(
                               secStartTick + DPDK::Clocks::get_ticks_per_sec()
                           );
        } else {
            // Wait until the timestamp of sending next Unit
            DPDK::Clocks::delay_until_ticks(
                secStartTick + DPDK::Clocks::delay_us_to_ticks(txUnits[txUnitIdx].offsetUS)
            );
        }
    }
    stat.procStat.MarkFinishCycling();

    /* DPDK::Info::display_eth_stats(nicPortID); */
}


GenApplication::GenApplication(int argc, char *argv[])
{
    try {
        cxxopts::Options options("bus_generator", "Options: <dpdk_opts> -- <app_opts>");

        options.add_options()
            ("h,help", "Print usage")
            ("goose", "The number of unique GOOSE to generate and the frequency", cxxopts::value<std::vector<int>>())
            ("sv80", "The number of unique SV with 80 points", cxxopts::value<int>())
            ("sv256", "The number of unique SV with 256 points", cxxopts::value<int>());

        auto result = options.parse(argc, argv);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            rte_exit(0, "");
        }

        if (result.count("goose")) {
            auto gooseOpts = result["goose"].as<std::vector<int>>();
            if (gooseOpts.size() != 2) {
                throw std::invalid_argument("The goose option is invalid, must be: N,M");
            }

            m_gooseNum = gooseOpts[0];
            m_gooseSendFreq = gooseOpts[1];
        }
        if (result.count("sv80")) {
            m_sv80Num = result["sv80"].as<int>();
        }
        if (result.count("sv256")) {
            m_sv256Num = result["sv256"].as<int>();
        }
    } catch (const std::exception &e) {
        std::cerr << "cxxopts: Error parsing options: " << e.what() << std::endl;
        throw;
    }
}

void GenApplication::DisplayStatistic()
{
    Console::CyclicStat::PrintTableHeader({"No-mbuf"});
    Console::CyclicStat::PrintTableRow("Main", m_stat.procStat)
                      << std::format(" {:<10} |\n", m_stat.errSendCnt);
}

void GenApplication::Run(StopVarType &doWork)
{
    // DPDK settings
    const unsigned MBUF_NUM = 512 * 1024,
                   CACHE_NUM = 64,
                   RX_DESC_NUM = 128,
                   TX_DESC_NUM = 63 * 1024;

    // Memory pool for skeletons
    DPDK::Mempool pool("bus_gen_pool", MBUF_NUM, CACHE_NUM);

    uint16_t nicPortID = 0, nicQueueID = 0;
    DPDK::Port port = DPDK::PortBuilder(nicPortID)
                            .SetMemPool(pool.Get())
                            .AdjustQueues(1, 1)
                            .SetDescriptors(RX_DESC_NUM, TX_DESC_NUM)
                            .Build();

    // Start NIC port
    port.Start();
    if (!port.WaitLink(10)) {
        throw std::runtime_error("Link is still down after 10 sec...");
    }

    TxCycleConfig conf{pool.Get(), port.GetID(), nicQueueID};

    // Main cycle
    if (m_gooseNum > 0) {
        // GOOSE
        const unsigned DEF_GOOSE_ENTRIES = 16;
        GooseTrafficGen gen(m_gooseNum, m_gooseSendFreq, DEF_GOOSE_ENTRIES);

        DPDK::PoolSetter(gen.GetSkeletonBuffer(), gen.GetSkeletonSize())
                        .FillPackets(pool.Get());

        // Generate cycle with GOOSE packets
        tx_packets_cycle< GooseTrafficGen >(conf, m_stat, gen, doWork);
    } else if (m_sv80Num > 0) {
        // SV 80 points
        SVTrafficGen gen(m_sv80Num, SV_TYPE::SV80);

        DPDK::PoolSetter(gen.GetSkeletonBuffer(), gen.GetSkeletonSize())
                        .FillPackets(pool.Get());

        // Generate cycle with SV packets
        tx_packets_cycle< SVTrafficGen, &SVTrafficGen::AmendPacketSV80 >(conf, m_stat, gen, doWork);
    } else if (m_sv256Num > 0) {
        // SV 256 points
        SVTrafficGen gen(m_sv256Num, SV_TYPE::SV256);

        DPDK::PoolSetter(gen.GetSkeletonBuffer(), gen.GetSkeletonSize())
                        .FillPackets(pool.Get());

        // Generate cycle with SV packets
        tx_packets_cycle< SVTrafficGen, &SVTrafficGen::AmendPacketSV256 >(conf, m_stat, gen, doWork);
    } else {
        std::cerr << "You have to specify GOOSE or SV to generate!\n";
    }

    // Finish delimiter
    std::cout << std::format("\n\n{:*<80}\n{:*^80}\n{:*<80}\n\n",
                             "", " FINISH ", "");

    port.Stop();
    DisplayStatistic();
}

