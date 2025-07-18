#include "rx_application.hpp"
#include "common/console_tables.hpp"

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
     * @function is_pbus_proto
     * @brief Is used to get APPID and dispatch GOOSE/SV mbuf to a particular CPU
     */
    inline BUS_PROTO is_pbus_proto(const uint8_t* buffer, unsigned *appid)
    {
        if (buffer[12] == 0x81 && buffer[13] == 0x00) {
            // VLAN
            if (buffer[16] == 0x88 && buffer[17] == 0xBA) {
                // SV
                *appid = get_appid(buffer + 18);
                return BUS_PROTO_SV;
            }
            if (buffer[16] == 0x88 && buffer[17] == 0xB8) {
                // GOOSE
                *appid = get_appid(buffer + 18);
                return BUS_PROTO_GOOSE;
            }
        }
        if (buffer[12] == 0x88 && buffer[13] == 0xBA) {
            // SV without VLAN
            *appid = get_appid(buffer + 14);
            return BUS_PROTO_SV;
        }
        if (buffer[12] == 0x88 && buffer[13] == 0xB8) {
            // GOOSE without VLAN
            *appid = get_appid(buffer + 14);
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
                              << "\n"
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
        conf->m_procStat.MarkStartCycling();
        while (g_doWork) {
            uint16_t rxNum = rte_ring_sc_dequeue_burst(conf->m_ring,
                                                       (void **)bufs,
                                                       BURST_SIZE,
                                                       nullptr);
            if (rxNum > 0) {
                conf->m_procStat.MarkProcBegin();
                for (uint16_t i=0;i<rxNum;++i) {
                    const uint8_t *packet = rte_pktmbuf_mtod(bufs[i], const uint8_t *);
                    const unsigned packetSize = rte_pktmbuf_pkt_len(bufs[i]);

                    unsigned appid = 0;
                    BUS_PROTO bt = is_pbus_proto(packet, &appid);

                    pipeline(*conf->m_app, bt, packet, packetSize);
                }
                rte_pktmbuf_free_bulk(bufs, rxNum);
                conf->m_procStat.MarkProcEnd();
            }
        }
        conf->m_procStat.MarkFinishCycling();
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

        std::cout << "\n\tStart main loop" << std::endl;
        /* set_thread_priority(DEF_PROCESS_PRIORITY); */

        // Main cycle
        DPDK::CyclicStat procStat;

        procStat.MarkStartCycling();
        rte_mbuf* bufs[BURST_SIZE] = { 0 };
        while (g_doWork) {
            uint16_t rxNum = rte_eth_rx_burst(eth.GetID(), queue_id, bufs, BURST_SIZE);
            if (rxNum > 0) {
                procStat.MarkProcBegin();

                for (unsigned i=0;i<rxNum;++i) {
                    const uint8_t *packet = rte_pktmbuf_mtod(bufs[i], const uint8_t *);

                    unsigned appid = 0;
                    BUS_PROTO bt = is_pbus_proto(packet, &appid);
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

                procStat.MarkProcEnd();
            }
        }
        procStat.MarkFinishCycling();

        // Finish delimiter
        std::cout << std::format("\n\n{:*<80}\n{:*^80}\n{:*<80}\n\n",
                                 "", " FINISH ", "");

        // Processing time
        Console::CyclicStat::PrintTableHeader();
        Console::CyclicStat::PrintTableRow("Main", procStat) << "\n";
    }

    void multi_core(RX_Application &app, DPDK::Port &eth, uint16_t queue_id)
    {
        const unsigned WORKER_RING_SIZE = 16 * 1024;

        // Pipeline workers
        std::vector< PipelineProcessor > pipelineWorker;
        pipelineWorker.reserve(rte_lcore_count());

        unsigned lcore = 0, wIndex = 0;
        RTE_LCORE_FOREACH_WORKER(lcore) {
            std::string ringName = "lcore_" + std::to_string(lcore);
            rte_ring *ring = rte_ring_create(ringName.c_str(),
                                             WORKER_RING_SIZE,
                                             rte_socket_id(),
                                             RING_F_SP_ENQ | RING_F_SC_DEQ);
            if (ring == nullptr) {
                throw std::runtime_error("Can't create ring for pipelineWorker: " + ringName);
            }

            pipelineWorker.push_back(PipelineProcessor(ring, &app, lcore));
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

        std::cout << "\n\tStart main loop with workers: " << workerNum << std::endl;
        /* set_thread_priority(DEF_PROCESS_PRIORITY); */

        // Workers' queues
        struct {
            rte_mbuf* buff[BURST_SIZE] = {};
            unsigned  num = 0;

            inline void Put(rte_mbuf *buf) {
                buff[num] = buf;
                ++num;
            }
        } workerQueue[RTE_MAX_LCORE] = {};

        // Main cycle
        DPDK::CyclicStat procStat;
        procStat.MarkStartCycling();
        rte_mbuf* bufs[BURST_SIZE] = { 0 };
        while (g_doWork) {
            uint16_t rxNum = rte_eth_rx_burst(eth.GetID(), queue_id, bufs, BURST_SIZE);
            if (rxNum > 0) {
                procStat.MarkProcBegin();

                for (unsigned i=0;i<rxNum;++i) {
                    const uint8_t *packet = rte_pktmbuf_mtod(bufs[i], const uint8_t *);

                    unsigned appid = get_appid(packet);
                    unsigned idx = appid & (workerNum - 1);

                    workerQueue[idx].Put(bufs[i]);
                }
                for (unsigned i=0;i<workerNum;++i) {
                    if (workerQueue[i].num > 0) {
                        rte_ring_sp_enqueue_burst(pipelineWorker[i].m_ring,
                                                  (void * const *)workerQueue[i].buff,
                                                  workerQueue[i].num,
                                                  NULL);
                        workerQueue[i].num = 0;
                    }
                }

                procStat.MarkProcEnd();
            }
        }
        procStat.MarkFinishCycling();

        // Finish delimiter
        std::cout << std::format("\n\n{:*<80}\n{:*^80}\n{:*<80}\n\n",
                                 "", " FINISH ", "");

        // Display processing time
        Console::CyclicStat::PrintTableHeader();
        Console::CyclicStat::PrintTableRow("Main", procStat) << "\n";
        for (const auto &w : pipelineWorker) {
            Console::CyclicStat::PrintTableRow("LCore" + std::to_string(w.m_lcore), w.m_procStat) << "\n";
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

    std::cout << "\n\tRX from ProcessBus configuration\n\n";

    if (m_confGooseNum > 0) {
        // Table header
        Console::GooseSource::PrintCfgTableHeader();

        for (unsigned i=0;i<m_confGooseNum;++i) {
            GooseSource::ptr src = std::make_shared< GooseSource >();
            src->SetMAC(MAC("01:0C:CD:04:00:01"))
                .SetAppID(0x0001 + i)
                .SetGOID(std::format("GOID{:08}", i + 1))
                .SetDataSetRef(std::format("IED{:08}LDName/LLN0$DataSet", i + 1))
                .SetGOCBRef(std::format("IED{:08}LDName/LLN0$GO$GOCB", i + 1))
                .SetCRev(1)
                .SetNumEntries(16);
            m_gooseMap[src->GetPassport()] = src;

            // Table row
            Console::GooseSource::PrintCfgTableRow(src);
        }
    }

    if (m_confSV80Num > 0) {
        // Table header
        Console::SVStreamSource::PrintCfgTableHeader();

        for (unsigned i=0;i<m_confSV80Num;++i) {
            SVStreamSource::ptr src = std::make_shared< SVStreamSource >();
            src->SetMAC(MAC("01:0C:CD:01:00:01"))
                .SetAppID(0x0001 + i)
                .SetSVID(std::format("SVID{:04}", i + 1))
                .SetCRev(1)
                .SetNumASDU(1);
            m_svMap[src->GetPassport()] = src;

            // Table row
            Console::SVStreamSource::PrintCfgTableRow(src);
        }
    }

    if (m_confSV256Num > 0) {
        // Table header
        Console::SVStreamSource::PrintCfgTableHeader();

        for (unsigned i=0;i<m_confSV256Num;++i) {
            SVStreamSource::ptr src = std::make_shared< SVStreamSource >();
            src->SetMAC(MAC("01:0C:CD:01:00:01"))
                .SetAppID(0x0001 + i)
                .SetSVID(std::format("SVID{:04}", i + 1))
                .SetCRev(1)
                .SetNumASDU(8);
            m_svMap[src->GetPassport()] = src;

            // Table row
            Console::SVStreamSource::PrintCfgTableRow(src);
        }
    }
}

void RX_Application::DisplayStatistic(unsigned interval_sec)
{
    #define BYTES_TO_MEGABITS(b)  ((b) * 8 / 1000000.0)
    #define BYTES_TO_MEGABYTES(b) ((b) / 1000000.0)

    rte_eth_stats start = m_lastPortStat;
    unsigned port_id = 0;
    rte_eth_stats_get(port_id, &m_lastPortStat);
    m_statDisplaySec += interval_sec;

    // Calculate RX and TX PPS/BPS
    uint64_t rx_pps = (m_lastPortStat.ipackets - start.ipackets) / interval_sec;
    uint64_t tx_pps = (m_lastPortStat.opackets - start.opackets) / interval_sec;
    uint64_t rx_bps = (m_lastPortStat.ibytes - start.ibytes) / interval_sec;
    uint64_t tx_bps = (m_lastPortStat.obytes - start.obytes) / interval_sec;

    std::cout << std::format("\nTime {} sec\n\n", m_statDisplaySec);

    rte_eth_stats stats;
    if (rte_eth_stats_get(port_id, &stats) == 0) {
        std::cout << std::format(
                        "            | RX         | TX         |\n"
                        "---------------------------------------\n"
                        "Load(Mbps)  | {:<10} | {:<10} |\n"
                        "PPS         | {:<10} | {:<10} |\n"
                        "Packets     | {:<10} | {:<10} |\n"
                        "Bytes       | {:<10} | {:<10} |\n"
                        "Errors      | {:<10} | {:<10} |\n"
                        "Missed      | {:<10} |            |\n"
                        "No-mbuf     | {:<10} |            |\n",
                        BYTES_TO_MEGABITS(rx_bps), BYTES_TO_MEGABITS(tx_bps),
                        rx_pps, tx_pps,
                        stats.ipackets, stats.opackets,
                        BYTES_TO_MEGABYTES(stats.ibytes), BYTES_TO_MEGABYTES(stats.obytes),
                        stats.ierrors, stats.oerrors,
                        stats.imissed,
                        stats.rx_nombuf
                    )
                  << std::endl;
    }

    // Proto information
    std::cout << std::format(
                        " Category   | GOOSE      | SV         |\n"
                        "---------------------------------------\n"
                        "{:<10}  | {:<10} | {:<10} |\n"
                        "{:<10}  | {:<10} | {:<10} |\n"
                        "{:<10}  | {:<10} | {:<10} |\n"
                        "{:<10}  | {:<10} | {:<10} |\n",
                        "Total",   m_rxGoosePktCnt, m_rxSVPktCnt,
                        "Error",   m_errGooseParserCnt, m_errSVParserCnt,
                        "Unknown", m_rxUnknownGooseCnt, m_rxUnknownSVCnt,
                        "Kernel",  "-", m_pktToKernelCnt
                 )
              << std::endl;
}

void RX_Application::DisplayResults()
{
    std::cout << std::endl;

    if (!m_gooseMap.empty()) {
        Console::GooseSource::PrintTableHeader();

        for (const auto &src : m_gooseMap) {
            GooseSource::ptr g = src.second;

            Console::GooseSource::PrintTableRow(g);
        }
    }

    if (!m_svMap.empty()) {
        Console::SVStreamSource::PrintTableHeader();

        for (const auto &src : m_svMap) {
            SVStreamSource::ptr s = src.second;

            Console::SVStreamSource::PrintTableRow(s);
        }
    }
}

void RX_Application::Run(StopVarType &doWork)
{
    // DPDK settings
    const unsigned MBUF_NUM = 512 * 1024,
                   CACHE_NUM = 64,
                   RX_DESC_NUM = 63 * 1024,
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

    // Common information
    /*
    DPDK::Info::display_lcore_info();
    DPDK::Info::display_eth_info();
    DPDK::Info::display_pools_info();
    */

    // Processing style
    switch (rte_lcore_count()) {
    case 1: {
        single_core(*this, eth, queue_id);
        break;
    }
    case 2: {
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

    /* std::cout << "Mempool: \n" << pool << std::endl; */
    DisplayResults();
}

