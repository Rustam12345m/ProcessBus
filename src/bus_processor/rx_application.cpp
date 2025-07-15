#include "rx_application.hpp"

#include "dpdk_cpp/dpdk_poolsetter_class.hpp"
#include "dpdk_cpp/dpdk_mempool_class.hpp"
#include "dpdk_cpp/dpdk_info_class.hpp"

#include "cxxopts.hpp"

const unsigned  BURST_SIZE = 32;

// TODO: Remove g_doWork
extern volatile bool g_doWork;

// TODO: Refactor functions locations
// Hack for inlineing these parsing functions
#include "process_bus_parser.cpp"
namespace 
{
    inline unsigned get_appid(const uint8_t* buffer)
    {
        return RTE_STATIC_BSWAP16(*(uint16_t *)(buffer + 18));
    }

    /**
     * @function is_bus_proto
     * @brief Is used to get APPID and dispatch GOOSE/SV mbuf to a particular CPU
     */
    inline BUS_PROTO is_bus_proto(const uint8_t* buffer, unsigned *appid)
    {
        if (buffer[12] == 0x81 && buffer[13] == 0x00 /* VLAN */ &&
                buffer[16] == 0x88 && buffer[17] == 0xBA /* SV */) {
            *appid = RTE_STATIC_BSWAP16(*(uint16_t *)(buffer + 18));
            return BUS_PROTO_SV;
        }
        if (buffer[12] == 0x81 && buffer[13] == 0x00 /* VLAN */ &&
                buffer[16] == 0x88 && buffer[17] == 0xB8 /* GOOSE */) {
            *appid = RTE_STATIC_BSWAP16(*(uint16_t *)(buffer + 18));
            return BUS_PROTO_GOOSE;
        }
        if (buffer[12] == 0x88 && buffer[13] == 0xBA) {
            // SV without VLAN
            *appid = RTE_STATIC_BSWAP16(*(uint16_t *)(buffer + 14));
            return BUS_PROTO_SV;
        }
        if (buffer[12] == 0x88 && buffer[13] == 0xB8) {
            // GOOSE without VLAN
            *appid = RTE_STATIC_BSWAP16(*(uint16_t *)(buffer + 14));
            return BUS_PROTO_GOOSE;
        }
        return NON_BUS_PROTO;
    }

    inline void pipeline(RX_Application &app, BUS_PROTO type,
                         const uint8_t *packet, size_t size)
    {
        // TODO:
        // - Pipeline style array of functions. Statically defined with X-macro
        // - Statistics is used from diffo threads in app

        switch (type) {
        case BUS_PROTO_GOOSE: {
            GoosePassport pass;
            GooseState state;

            int retval = parse_goose_packet(packet, size, pass, state);
            if (retval == 0) {
                auto src = app.m_gooseMap.find(pass);
                if (src != app.m_gooseMap.end()) {
                    src->second->ProcessState(pass, state);

                    ++app.m_rxGoosePktCnt;
                } else {
                    ++app.m_rxUnknownGooseCnt;

                    /*
                    std::cout << "Received unknown GOOSE: " << retval << "\n"
                            << "PacketLen = " << size << "\n"
                            << pass
                            << state
                            << std::endl;
                    display_packet_as_array(packet, size);
                    */
                }
            } else {
                // Invalid GOOSE packet
                ++app.m_errGooseParserCnt;
            }
            break;
        }
        case BUS_PROTO_SV: {
            SVStreamPassport pass;
            SVStreamState state;

            int retval = parse_sv_packet(packet, size, pass, state);
            if (retval == 0) {
                auto src = app.m_svMap.find(pass);
                if (src != app.m_svMap.end()) {
                    src->second->ProcessState(pass, state);

                    ++app.m_rxSVPktCnt;
                } else {
                    ++app.m_rxUnknownSVCnt;
                }
            } else {
                ++app.m_errSVParserCnt;
            }
            break;
        }
        case NON_BUS_PROTO: {
            break;
        }
        }
    }

    int lcore_processor(void *arg)
    {
        PipelineProcessor *conf = reinterpret_cast< PipelineProcessor* >(arg);
        if (conf == nullptr) {
            g_doWork = false;
            std::cerr << "LCore: Config is NULL!" << std::endl;
            return -1;
        }

        // CPU core by DPDK's cmd
        /* set_thread_priority(DEF_WORKER_PRIORITY); */

        rte_mbuf *bufs[BURST_SIZE] = {};
        conf->m_workStat.MarkStartCycling();
        while (g_doWork) {
            uint16_t rxNum = rte_ring_sc_dequeue_burst(conf->m_ring,
                                                       (void **)bufs,
                                                       BURST_SIZE,
                                                       nullptr);
            if (rxNum > 0) {
                conf->m_workStat.MarkProcBegin();
                for (uint16_t i=0;i<rxNum;++i) {
                    const uint8_t *packet = rte_pktmbuf_mtod(bufs[i], const uint8_t *);
                    const unsigned packetSize = rte_pktmbuf_pkt_len(bufs[i]);

                    unsigned appid = 0;
                    BUS_PROTO bt = is_bus_proto(packet, &appid);

                    pipeline(*conf->m_app, bt, packet, packetSize);
                }
                rte_pktmbuf_free_bulk(bufs, rxNum);
                conf->m_workStat.MarkProcEnd();
            }
        }
        conf->m_workStat.MarkFinishCycling();
        return 0;
    }

    void single_core(RX_Application &app, DPDK::Port &eth, uint16_t queue_id)
    {
        // Start NIC port
        eth.SetAllMulticast();
        eth.Start();
        if (!eth.WaitLink(10)) {
            g_doWork = false;

            throw std::runtime_error("Link is still down after 10 sec...");
        }

        std::cout << "Start main loop" << std::endl;
        /* set_thread_priority(DEF_PROCESS_PRIORITY); */

        // Main cycle
        DPDK::CyclicStat workStat;

        workStat.MarkStartCycling();
        rte_mbuf* bufs[BURST_SIZE] = { 0 };
        while (g_doWork) {
            uint16_t rxNum = rte_eth_rx_burst(eth.GetID(), queue_id, bufs, BURST_SIZE);
            if (rxNum > 0) {
                workStat.MarkProcBegin();

                for (unsigned i=0;i<rxNum;++i) {
                    const uint8_t *packet = rte_pktmbuf_mtod(bufs[i], const uint8_t *);

                    unsigned appid = 0;
                    BUS_PROTO bt = is_bus_proto(packet, &appid);
                    switch (bt) {
                    case NON_BUS_PROTO: {
                        // TODO: Dispatch to: Thread - Kernel(TAP)
                        ++app.m_pktToKernelCnt;
                        break;
                    }
                    case BUS_PROTO_SV:
                    case BUS_PROTO_GOOSE: {
                        pipeline(app, bt, packet, rte_pktmbuf_pkt_len(bufs[i]));
                        break;
                    }
                    }
                    rte_pktmbuf_free(bufs[i]);
                }

                workStat.MarkProcEnd();
            }
        }
        workStat.MarkFinishCycling();

        std::cout << "Processing(main): \n" << workStat
                  << std::endl;
    }

    void multi_core(RX_Application &app, DPDK::Port &eth, uint16_t queue_id)
    {
        const unsigned WORKER_RING_SIZE = 16 * 1024;

        // Pipeline workers
        unsigned lcore = 0, wIndex = 0;
        std::vector< PipelineProcessor > pipelineWorker;
        pipelineWorker.reserve(rte_lcore_count());

        RTE_LCORE_FOREACH_WORKER(lcore) {
            std::string ringName = "lcore_thread" + std::to_string(lcore);
            rte_ring *ring = rte_ring_create(ringName.c_str(),
                                             WORKER_RING_SIZE,
                                             rte_socket_id(),
                                             RING_F_SP_ENQ | RING_F_SC_DEQ);
            if (ring == nullptr) {
                throw std::runtime_error("Can't create ring for pipelineWorker: " + ringName);
            }

            pipelineWorker.emplace_back(ring, &app, lcore);
            rte_eal_remote_launch(lcore_processor, &pipelineWorker[wIndex], lcore);
            ++wIndex;
        }
        const unsigned workerNum = pipelineWorker.size();

        // Start NIC port
        eth.SetAllMulticast();
        eth.Start();
        if (!eth.WaitLink(10)) {
            g_doWork = false;

            throw std::runtime_error("Link is still down after 10 sec...");
        }

        std::cout << "Start main loop with pipelineWorkers: " << workerNum << std::endl;
        /* set_thread_priority(DEF_PROCESS_PRIORITY); */

        // Workers
        rte_ring* WRings[RTE_MAX_LCORE] = {};
        for (int i=0;i<workerNum;++i) {
            WRings[i] = pipelineWorker[i].m_ring;
        }
        rte_mbuf* WorkerBufs[RTE_MAX_LCORE][BURST_SIZE] = {};
        unsigned WorkerBufsSize[BURST_SIZE] = {};

        // Main cycle
        DPDK::CyclicStat workStat;
        workStat.MarkStartCycling();

        rte_mbuf* bufs[BURST_SIZE] = { 0 };
        while (g_doWork) {
            uint16_t rxNum = rte_eth_rx_burst(eth.GetID(), queue_id, bufs, BURST_SIZE);
            if (rxNum > 0) {
                workStat.MarkProcBegin();

                for (unsigned i=0;i<rxNum;++i) {
                    const uint8_t *packet = rte_pktmbuf_mtod(bufs[i], const uint8_t *);

                    unsigned appid = get_appid(packet);
                    unsigned workerIdx = appid & (workerNum - 1);
                    WorkerBufs[workerIdx][WorkerBufsSize[workerIdx]] = bufs[i];
                    ++WorkerBufsSize[workerIdx];
                }
                for (unsigned i=0;i<workerNum;++i) {
                    if (WorkerBufsSize[i] > 0) {
                        rte_ring_sp_enqueue_burst(WRings[i],
                                (void * const *)WorkerBufs[i],
                                WorkerBufsSize[i],
                                NULL);
                        WorkerBufsSize[i] = 0;
                    }
                }

                workStat.MarkProcEnd();
            }
        }
        workStat.MarkFinishCycling();

        std::cout << "Processing(main): \n" << workStat
                  << std::endl;
        for (const auto &w : pipelineWorker) {
            std::cout << "Worker: [" << w.m_lcore << "]\n"
                      << w.m_workStat << "\n"
                      << "\tNoFreeDesc = " << w.m_noFreeDesc << "\n"
                      << std::endl;
        }
    }
}


RX_Application::RX_Application(int argc, char *argv[])
{
    Init(argc, argv);
}

void RX_Application::ParseCmdOptions(int argc, char* argv[])
{
    try {
        cxxopts::Options options("bus_processor", "Options: <dpdk_opts> -- <app_opts>");

        options.add_options()
            ("h,help", "Print usage")
            ("goose", "The number of unique GOOSE is being reserved", cxxopts::value< int >())
            ("sv80", "The number of unique SV with 80 points", cxxopts::value< int >())
            ("sv256", "The number of unique SV with 256 points", cxxopts::value< int >());

        auto result = options.parse(argc, argv);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            rte_exit(0, "");
        }

        if (result.count("goose")) {
            m_confGooseNum = result["goose"].as< int >();
        }
        if (result.count("sv80")) {
            m_confSV80Num = result["sv80"].as< int >();
        }
        if (result.count("sv256")) {
            m_confSV256Num = result["sv256"].as< int >();
        }
    } catch (const std::exception &e) {
        std::cerr << "cxxopts: Error parsing options: " << e.what() << std::endl;
        throw;
    }
}

void RX_Application::Init(int argc, char* argv[])
{
    ParseCmdOptions(argc, argv);

    for (unsigned i=0;i<m_confGooseNum;++i) {
        GooseSource::ptr src = std::make_shared< GooseSource >();
        src->SetMAC(MAC("01:0C:CD:04:00:00"))
            .SetAppID(0x0001 + i)
            .SetGOID(std::format("GOID{:08}", i + 1))
            .SetDataSetRef(std::format("IED{:08}LDName/LLN0$DataSet", i + 1))
            .SetGOCBRef(std::format("IED{:08}LDName/LLN0$GO$GOCB", i + 1))
            .SetCRev(1)
            .SetNumEntries(16);
        m_gooseMap[src->GetPassport()] = src;

        std::cout << "Configured GOOSE[" << i + 1 << "]:\n"
                  << src->GetPassport()
                  << std::endl;
    }

    for (unsigned i=0;i<m_confSV80Num;++i) {
        SVStreamSource::ptr src = std::make_shared< SVStreamSource >();
        src->SetMAC(MAC("01:0C:CD:01:00:01"))
            .SetAppID(0x0001 + i)
            .SetSVID(std::format("SVID{:04}", i + 1))
            .SetCRev(1)
            .SetNumASDU(1);
        m_svMap[src->GetPassport()] = src;

        std::cout << "Configured SV80[" << i + 1 << "]:\n"
                  << src->GetPassport()
                  << std::endl;
    }

    for (unsigned i=0;i<m_confSV256Num;++i) {
        SVStreamSource::ptr src = std::make_shared< SVStreamSource >();
        src->SetMAC(MAC("01:0C:CD:01:00:01"))
            .SetAppID(0x0001 + i)
            .SetSVID(std::format("SVID{:04}", i + 1))
            .SetCRev(1)
            .SetNumASDU(8);
        m_svMap[src->GetPassport()] = src;

        std::cout << "Configured SV256[" << i + 1 << "]:\n"
                  << src->GetPassport()
                  << std::endl;
    }
}

void RX_Application::DisplayStatistic(unsigned interval_sec)
{
    #define BYTES_TO_MEGABITS(b)  ((b) * 8 / 1000000.0)
    #define BYTES_TO_MEGABYTES(b) ((b) / 1000000.0)

    unsigned port_id = 0;

    rte_eth_stats start = m_lastPortStat;
    rte_eth_stats_get(port_id, &m_lastPortStat);
    m_statDisplaySec += interval_sec;

    // Calculate RX and TX PPS/BPS
    uint64_t rx_pps = (m_lastPortStat.ipackets - start.ipackets) / interval_sec;
    uint64_t tx_pps = (m_lastPortStat.opackets - start.opackets) / interval_sec;

    uint64_t rx_bps = (m_lastPortStat.ibytes - start.ibytes) / interval_sec;
    uint64_t tx_bps = (m_lastPortStat.obytes - start.obytes) / interval_sec;

    std::cout << std::format(
                    "\nStatistics: Port {} (interval: {} sec, current: {} sec):\n"
                    "\tRX-PPS: {}, RX-BPS: {} ({:.2f} Mbps, {:.2f} MBps)\n"
                    "\tTX-PPS: {}, TX-BPS: {} ({:.2f} Mbps, {:.2f} MBps)\n",
                    port_id, interval_sec, m_statDisplaySec,
                    rx_pps, rx_bps, BYTES_TO_MEGABITS(rx_bps), BYTES_TO_MEGABYTES(rx_bps),
                    tx_pps, tx_bps, BYTES_TO_MEGABITS(tx_bps), BYTES_TO_MEGABYTES(tx_bps)
                 )
              << std::endl;

    rte_eth_stats stats;
    if (rte_eth_stats_get(port_id, &stats) == 0) {
        std::cout << std::format(
                        "\tRX-packets: {}\n"
                        "\tTX-packets: {}\n"
                        "\tRX-bytes:   {}\n"
                        "\tTX-bytes:   {}\n"
                        "\tRX-errors:  {}\n"
                        "\tTX-errors:  {}\n"
                        "\tRX-missed:  {}\n"
                        "\tRX-no-mbuf: {}\n",
                        stats.ipackets, stats.opackets, stats.ibytes, stats.obytes,
                        stats.ierrors, stats.oerrors, stats.imissed, stats.rx_nombuf
                     )
                  << std::endl;
    }

    std::cout << "\tGOOSE:\n"
              << "\t\tTotal:    " << m_rxGoosePktCnt << "\n"
              << "\t\tError:    " << m_errGooseParserCnt << "\n"
              << "\t\tUnknown:  " << m_rxUnknownGooseCnt << "\n"
              << "\tSV:\n"
              << "\t\tTotal:    " << m_rxSVPktCnt << "\n"
              << "\t\tError:    " << m_errSVParserCnt << "\n"
              << "\t\tUnknown:  " << m_rxUnknownSVCnt << "\n"
              << "\t\tToKernel: " << m_pktToKernelCnt
              << std::endl;
}

void RX_Application::Run(StopVarType &doWork)
{
    const unsigned MBUF_NUM = 64 * 1024,
                   CACHE_NUM = 64,
                   RX_DESC_NUM = 8 * 1024,
                   TX_DESC_NUM = 128;

    // Create memory pool
    DPDK::Mempool pool("bus_proc_pool", MBUF_NUM, CACHE_NUM);

    // Create Ethernet port
    uint16_t port_id = 0, queue_id = 0;
    DPDK::Port eth = DPDK::PortBuilder(port_id)
                            .SetMemPool(pool.Get())
                            .AdjustQueues(1, 1)
                            .SetDescriptors(RX_DESC_NUM, TX_DESC_NUM)
                            .Build();
    std::cout << eth << std::endl;

    // Common information
    /*
    DPDK::Info::display_lcore_info();
    DPDK::Info::display_eth_info();
    DPDK::Info::display_pools_info();
    */

    // Processing style
    switch (rte_lcore_count()) {
    case 0: {
        // Single core without sRSS
        single_core(*this, eth, queue_id);
        break;
    }
    case 1: {
        // Single core + 1 lcore for sRSS
        break;
    }
    default: {
        // Software RSS
        multi_core(*this, eth, queue_id);
        break;
    }
    }

    // Stop all
    eth.Stop();
    rte_eal_mp_wait_lcore();

    for (const auto &src : m_gooseMap) {
        std::cout << "GOOSE: \n" << *src.second << std::endl;
    }
    for (const auto &src : m_svMap) {
        std::cout << "SV: \n" << *src.second << std::endl;
    }
    std::cout << "Mempool: \n" << pool << std::endl;
}

