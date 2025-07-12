#include "dpdk_cpp/dpdk_port_class.hpp"
#include "dpdk_cpp/dpdk_cyclestat_class.hpp"
#include "dpdk_cpp/dpdk_poolsetter_class.hpp"
#include "dpdk_cpp/dpdk_mempool_class.hpp"
#include "dpdk_cpp/dpdk_info_class.hpp"

#include "goose_traffic_gen.hpp"
#include "sv_traffic_gen.hpp"
#include "common/utils.hpp"
#include "common/shared_defs.hpp"

#include "cxxopts.hpp"

#include <signal.h>
#include <atomic>

volatile bool g_doWork = true;

void signal_handler(int)
{
    g_doWork = false;
}

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

template<
    typename GenClass,
    size_t (GenClass::*Amend)(uint8_t *packet, const typename GenClass::Desc &desc)
            = &GenClass::AmendPacket
>
static void gen_process(rte_mempool *pool, uint16_t port_id,
                        uint16_t queue_id, GenClass &gen)
{
    typename GenClass::TxUnitArray &txUnits = gen.GetTxUnits();

    display_tx_units_info< typename GenClass::TxUnitArray >(txUnits);

    const unsigned BURST_SIZE = 32;
    rte_mbuf* mbufs[BURST_SIZE] = { 0 };

    std::cout << "Start main loop\n";
    /* set_thread_priority(DEF_GENERATOR_PRIORITY); */

    // Main cycle
    DPDK::CyclicStat workStat;
    workStat.MarkStartCycling();
    uint64_t secStartTick = workStat.GetStartTick();
    unsigned txUnitIdx = 0, cantSendCnt = 0;
    while (g_doWork) {
        workStat.MarkProcBegin();
        for (const auto &blk : txUnits[txUnitIdx].blocks) {
            unsigned count = blk.packets.size(), sendNum = 0;
            while (sendNum < count) {
                unsigned num = ((count - sendNum) >= BURST_SIZE) ? BURST_SIZE
                                                                 : (count - sendNum);
                if (rte_pktmbuf_alloc_bulk(pool, mbufs, num) == 0) {
                    for (size_t i=0;i<num;++i) {
                        uint8_t *packet = rte_pktmbuf_mtod(mbufs[i], uint8_t *);

                        mbufs[i]->pkt_len = (gen.*Amend)(packet, blk.packets[sendNum + i]);
                        mbufs[i]->data_len = mbufs[i]->pkt_len;
                    }

                    uint16_t nb_tx = rte_eth_tx_burst(port_id, queue_id, mbufs, num);
                    if (nb_tx < num) {
                        for (uint16_t i=nb_tx;i<num;i++) {
                            rte_pktmbuf_free(mbufs[i]);
                        }
                        cantSendCnt += num - nb_tx;
                    }
                } else {
                    rte_eth_tx_done_cleanup(port_id, queue_id, 0);
                    continue;
                }

                sendNum += num;
            }
        }
        workStat.MarkProcEnd();

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
    workStat.MarkFinishCycling();

    std::cout << "\nProcessing statistics:\n" << workStat << std::endl;
    std::cout << "\tCantSendCnt = " << cantSendCnt << std::endl;
    /* DPDK::Info::display_eth_stats(port_id); */
}

static void main_thread(int argc, char *argv[])
{
    // App's options
    unsigned gooseNum = 0, gooseSendFreq = 1, sv80Num = 0, sv256Num = 0;
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
            return;
        }

        if (result.count("goose")) {
            auto gooseOpts = result["goose"].as<std::vector<int>>();
            if (gooseOpts.size() != 2) {
                throw std::invalid_argument("The goose option is invalid, must be: N,M");
            }

            gooseNum = gooseOpts[0];
            gooseSendFreq = gooseOpts[1];
        }
        if (result.count("sv80")) {
            sv80Num = result["sv80"].as<int>();
        }
        if (result.count("sv256")) {
            sv256Num = result["sv256"].as<int>();
        }
    } catch (const std::exception &e) {
        std::cerr << "cxxopts: Error parsing options: " << e.what() << std::endl;
        throw;
    }

    // Memory pool for skeletons
    const unsigned MBUF_NUM = 64 * 1024, CACHE_NUM = 64;
    DPDK::Mempool pool("bus_gen_pool", MBUF_NUM, CACHE_NUM);

    uint16_t port_id = 0, queue_id = 0;
    const unsigned RX_DESC_NUM = 1 * 1024, TX_DESC_NUM = 8 * 1024;
    DPDK::Port eth = DPDK::PortBuilder(port_id)
                            .SetMemPool(pool.Get())
                            .AdjustQueues(1, 1)
                            .SetDescriptors(RX_DESC_NUM, TX_DESC_NUM)
                            .Build();

    // Thread identity, CPU core by DPDK's command
    set_thread_name("main");

    // Start NIC port
    eth.Start();
    if (!eth.WaitLink(10)) {
        throw std::runtime_error("Link is still down after 10 sec...");
    }

    if (gooseNum > 0) {
        // GOOSE
        const unsigned DEF_GOOSE_ENTRIES = 16;
        GooseTrafficGen gen(gooseNum, gooseSendFreq, DEF_GOOSE_ENTRIES);
        DPDK::PoolSetter(gen.GetSkeletonBuffer(), gen.GetSkeletonSize())
                .FillPackets(pool.Get());

        gen_process< GooseTrafficGen >(pool.Get(), port_id, queue_id, gen);
    } else if (sv80Num > 0) {
        // SV 80 points
        SVTrafficGen gen(sv80Num, SV_TYPE::SV80);
        DPDK::PoolSetter(gen.GetSkeletonBuffer(), gen.GetSkeletonSize())
                .FillPackets(pool.Get());

        gen_process< SVTrafficGen, &SVTrafficGen::AmendPacketSV80 >(
            pool.Get(), port_id, queue_id, gen
        );
    } else if (sv256Num > 0) {
        // SV 256 points
        SVTrafficGen gen(sv256Num, SV_TYPE::SV256);
        DPDK::PoolSetter(gen.GetSkeletonBuffer(), gen.GetSkeletonSize())
                .FillPackets(pool.Get());

        gen_process< SVTrafficGen, &SVTrafficGen::AmendPacketSV256 >(
            pool.Get(), port_id, queue_id, gen
        );
    } else {
        std::cerr << "You have to specify GOOSE or SV to generate!\n";
    }

    eth.Stop();
}

int main(int argc, char *argv[])
{
    // Signals to finish processing
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    // Initialize the Environment Abstraction Layer (EAL)
    int retval = rte_eal_init(argc, argv);
    if (retval < 0) {
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
    }
    // Skip DPDK's options
    argc -= retval;
    argv += retval;

    if (rte_eth_dev_count_avail() == 0) {
        rte_exit(EXIT_FAILURE, "No available ports. Check port binding.\n");
    }
    if (rte_get_main_lcore() == 0) {
        rte_exit(EXIT_FAILURE, "You can't use core 0 to generate/process BUSes!\n");
    }

    // Packet generator
    try {
        main_thread(argc, argv);
    } catch (const std::exception &exp) {
        g_doWork = false;
        std::cerr << "Exception: " << exp.what() << std::endl;
    }

    rte_eal_cleanup();
    return 0;
}

