#include "dpdk_cpp/dpdk_port_class.hpp"
#include "dpdk_cpp/dpdk_cyclestat_class.hpp"
#include "dpdk_cpp/dpdk_poolsetter_class.hpp"
#include "dpdk_cpp/dpdk_mempool_class.hpp"
#include "dpdk_cpp/dpdk_info_class.hpp"

#include "process_bus_parser.hpp"
#include "common/goose_container.hpp"
#include "common/utils.hpp"
#include "common/shared_defs.hpp"

#include "cxxopts.hpp"

#include <signal.h>
#include <atomic>
#include <stdexcept>

enum BUS_PROTO_TYPE
{
    NON_BUS_PROTO = 0,
    BUS_PROTO_SV,
    BUS_PROTO_GOOSE,
    /* BUS_PROTO_PTP */
};

/**
 * @function is_bus_proto
 * @brief Is used to get APPID and dispatch GOOSE/SV mbuf to a particular CPU
 */
inline BUS_PROTO_TYPE is_bus_proto(const uint8_t* buffer, unsigned *appid)
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

struct LCoreWorker
{
    rte_ring*   m_ring = nullptr;
    unsigned    m_lcore = 0;
    DPDK::CyclicStat m_workStat;
    uint64_t    m_noFreeDesc = 0;

    LCoreWorker(rte_ring *ring, unsigned lcore)
        : m_ring(ring), m_lcore(lcore)
    {}
};

struct Application
{
    void ParseCmdOptions(int argc, char* argv[]) {
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

    void Init(int argc, char* argv[]) {
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

    inline void ProcessBusFrame(BUS_PROTO_TYPE bt, const uint8_t *packet, size_t packetSize) {
        switch (bt) {
        case BUS_PROTO_GOOSE: {
            GoosePassport pass;
            GooseState state;

            int retval = parse_goose_packet(packet, packetSize, pass, state);
            if (retval == 0) {
                auto src = m_gooseMap.find(pass);
                if (src != m_gooseMap.end()) {
                    src->second->ProcessState(pass, state);

                    ++m_rxGoosePktCnt;
                } else {
                    ++m_rxUnknownGooseCnt;
                    /*
                       std::cout << "Received unknown GOOSE:\n"
                       << pass << state << std::endl;
                    */
                }
            } else {
                // Invalid GOOSE packet
                ++m_errGooseParserCnt;
            }
            break;
        }
        case BUS_PROTO_SV: {
            SVStreamPassport pass;
            SVStreamState state;

            int retval = parse_sv_packet(packet, packetSize, pass, state);
            if (retval == 0) {
                auto src = m_svMap.find(pass);
                if (src != m_svMap.end()) {
                    src->second->ProcessState(pass, state);

                    ++m_rxSVPktCnt;
                } else {
                    ++m_rxUnknownSVCnt;
                }
            } else {
                ++m_errSVParserCnt;
            }
            break;
        }
        case NON_BUS_PROTO: {
            break;
        }
        }
    }

public:
    // Settings
    unsigned        m_confGooseNum = 0,
                    m_confSV80Num = 0,
                    m_confSV256Num = 0;

    // GOOSE
    GooseContainer  m_gooseMap;
    SVContainer     m_svMap;

    // Proto statistics
    uint64_t        m_rxGoosePktCnt = 0, m_rxSVPktCnt = 0,
                    m_errGooseParserCnt = 0, m_errSVParserCnt = 0,
                    m_rxUnknownGooseCnt = 0, m_rxUnknownSVCnt = 0,
                    m_pktToKernelCnt = 0;

    // Runtime
    volatile bool   m_doWork = true;
    std::vector< LCoreWorker > m_worker;
} g_app;

void signal_handler(int)
{
    g_app.m_doWork = false;
}

static void* stat_thread(void* args)
{
    #define BYTES_TO_MEGABITS(b)  ((b) * 8 / 1000000.0)
    #define BYTES_TO_MEGABYTES(b) ((b) / 1000000.0)

    unsigned port_id = 0, interval_sec = 2, current_sec = 0;

    while (g_app.m_doWork) {
        rte_eth_stats start, finish;

        rte_eth_stats_get(port_id, &start);
        sleep(interval_sec); // Wait
        rte_eth_stats_get(port_id, &finish);
        current_sec += interval_sec;

        // Calculate RX and TX PPS/BPS
        uint64_t rx_pps = (finish.ipackets - start.ipackets) / interval_sec;
        uint64_t tx_pps = (finish.opackets - start.opackets) / interval_sec;

        uint64_t rx_bps = (finish.ibytes - start.ibytes) / interval_sec;
        uint64_t tx_bps = (finish.obytes - start.obytes) / interval_sec;

        std::cout << std::format(
                        "\nStatistics: Port {} (interval: {} sec, current: {} sec):\n"
                        "\tRX-PPS: {}, RX-BPS: {} ({:.2f} Mbps, {:.2f} MBps)\n"
                        "\tTX-PPS: {}, TX-BPS: {} ({:.2f} Mbps, {:.2f} MBps)\n",
                        port_id, interval_sec, current_sec,
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
                  << "\t\tTotal:    " << g_app.m_rxGoosePktCnt << "\n"
                  << "\t\tError:    " << g_app.m_errGooseParserCnt << "\n"
                  << "\t\tUnknown:  " << g_app.m_rxUnknownGooseCnt << "\n"
                  << "\tSV:\n"
                  << "\t\tTotal:    " << g_app.m_rxSVPktCnt << "\n"
                  << "\t\tError:    " << g_app.m_errSVParserCnt << "\n"
                  << "\t\tUnknown:  " << g_app.m_rxUnknownSVCnt << "\n"
                  << "\t\tToKernel: " << g_app.m_pktToKernelCnt
                  << std::endl;
    }
    return NULL;
}

static int lcore_processor(void *arg)
{
    LCoreWorker *conf = reinterpret_cast< LCoreWorker* >(arg);
    if (conf == nullptr) {
        g_app.m_doWork = false;
        std::cerr << "LCore: Config is NULL!" << std::endl;
        return -1;
    }

    set_thread_priority(DEF_WORKER_PRIORITY);

    const unsigned BURST_SIZE = 32;
    rte_mbuf *bufs[BURST_SIZE] = {};
    conf->m_workStat.MarkStartCycling();
    while (g_app.m_doWork) {
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
                BUS_PROTO_TYPE bt = is_bus_proto(packet, &appid);
                g_app.ProcessBusFrame(bt, packet, packetSize);
            }
            rte_pktmbuf_free_bulk(bufs, rxNum);
            conf->m_workStat.MarkProcEnd();
        }
    }
    conf->m_workStat.MarkFinishCycling();
    return 0;
}

static void main_thread(int argc, char *argv[])
{
    // App's options
    g_app.Init(argc, argv);

    // Create memory pool
    const unsigned MBUF_NUM = 64 * 1024, CACHE_NUM = 64;
    DPDK::Mempool pool("bus_proc_pool", MBUF_NUM, CACHE_NUM);

    // Create Ethernet port
    const unsigned RX_DESC_NUM = 8 * 1024, TX_DESC_NUM = 128;
    uint16_t port_id = 0, queue_id = 0;
    DPDK::Port eth = DPDK::PortBuilder(port_id)
                            .SetMemPool(pool.Get())
                            .AdjustQueues(1, 1)
                            .SetDescriptors(RX_DESC_NUM, TX_DESC_NUM)
                            .SetPromisc()
                            .Build();

    std::cout << eth << std::endl;

    // Common information
    /*
    DPDK::Info::display_lcore_info();
    DPDK::Info::display_eth_info();
    DPDK::Info::display_pools_info();
    */

    // Statistics
    pthread_t statThHandle;
    pthread_create(&statThHandle, NULL, stat_thread, NULL);

    // Pin to CPU & RT priority
    /* pin_thread_to_cpu(DEF_BUS_RX_CPU, DEF_PROCESS_PRIORITY); */
    set_thread_priority(DEF_PROCESS_PRIORITY);

    // Workers
    const unsigned WORKET_RING_SIZE = 16 * 1024;
    unsigned lcore = 0, wIndex = 0;
    g_app.m_worker.reserve(rte_lcore_count());
    RTE_LCORE_FOREACH_WORKER(lcore) {
        std::string ringName = "m_worker_ring_" + std::to_string(lcore);
        rte_ring *ring = rte_ring_create(ringName.c_str(),
                                         WORKET_RING_SIZE,
                                         rte_socket_id(),
                                         RING_F_SP_ENQ | RING_F_SC_DEQ);
        if (ring == nullptr) {
            throw std::runtime_error("Can't create ring for m_worker: " + ringName);
        }

        g_app.m_worker.push_back(LCoreWorker(ring, lcore));
        rte_eal_remote_launch(lcore_processor, &g_app.m_worker[wIndex], lcore);
        ++wIndex;
    }
    const unsigned workerNum = g_app.m_worker.size();

    // Start NIC port
    eth.Start();
    if (!eth.WaitLink(10)) {
        throw std::runtime_error("Link is still down after 10 sec...");
    }

    std::cout << "Start main loop with m_workers: " << workerNum << std::endl;

    const unsigned BURST_SIZE = 32;
    rte_mbuf* bufs[BURST_SIZE] = { 0 };

    DPDK::CyclicStat workStat;
    workStat.MarkStartCycling();
    while (g_app.m_doWork) {
        uint16_t rxNum = rte_eth_rx_burst(eth.GetID(), queue_id, bufs, BURST_SIZE);
        if (rxNum > 0) {
            workStat.MarkProcBegin();

            for (unsigned i=0;i<rxNum;++i) {
                const uint8_t *packet = rte_pktmbuf_mtod(bufs[i], const uint8_t *);

                unsigned appid = 0;
                BUS_PROTO_TYPE bt = is_bus_proto(packet, &appid);
                switch (bt) {
                case NON_BUS_PROTO: {
                    // TODO: Dispatch to: Thread - Kernel(TAP)
                    ++g_app.m_pktToKernelCnt;
                    break;
                }
                case BUS_PROTO_SV:
                case BUS_PROTO_GOOSE: {
                    if (workerNum > 0) {
                        // Software RSS by APPID in GOOSE or SV
                        unsigned workerByAppID = appid & (workerNum - 1);

                        if (rte_ring_sp_enqueue(g_app.m_worker[workerByAppID].m_ring, bufs[i]) == 0) {
                            continue;
                        } else {
                            ++g_app.m_worker[workerByAppID].m_noFreeDesc;
                        }
                    } else {
                        // Handling without lcores
                        g_app.ProcessBusFrame(bt, packet, rte_pktmbuf_pkt_len(bufs[i]));
                    }
                    break;
                }
                }
                rte_pktmbuf_free(bufs[i]);
            }

            workStat.MarkProcEnd();
        }
    }
    workStat.MarkFinishCycling();

    eth.Stop();
    rte_eal_mp_wait_lcore();
    pthread_join(statThHandle, nullptr);

    for (const auto &src : g_app.m_gooseMap) {
        std::cout << "GOOSE: \n" << *src.second << std::endl;
    }
    for (const auto &src : g_app.m_svMap) {
        std::cout << "SV: \n" << *src.second << std::endl;
    }

    std::cout << "\nProcessing(main):\n" << workStat << std::endl;
    for (const auto &w : g_app.m_worker) {
        std::cout << "Worker: [" << w.m_lcore << "]\n"
                  << w.m_workStat << "\n"
                  << "\tNoFreeDesc = " << w.m_noFreeDesc << "\n"
                  << std::endl;
    }
    std::cout << "Mempool: \n" << pool << std::endl;
    /* DPDK::Info::display_eth_stats(port_id); */
}

int main(int argc, char *argv[])
{
    int retval = rte_eal_init(argc, argv);
    if (retval < 0) {
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
    }
    // Skip DPDK's options
    argc -= retval;
    argv += retval;

    if (rte_eth_dev_count_avail() == 0) {
        rte_exit(EXIT_FAILURE, "No available ports. Please, check port binding.\n");
    }
    if (rte_get_main_lcore() == 0) {
        rte_exit(EXIT_FAILURE, "You can't use core 0 to generate/process BUSes!\n");
    }

    // Signals to finish processing
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    // Packet receiver
    try {
        main_thread(argc, argv);
    } catch (const std::exception &exp) {
        std::cerr << "Exception: " << exp.what() << std::endl;
    }

    rte_eal_cleanup();
    return 0;
}

