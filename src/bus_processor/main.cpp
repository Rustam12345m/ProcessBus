#include "dpdk_cpp/dpdk_port_class.hpp"
#include "dpdk_cpp/dpdk_cyclestat_class.hpp"
#include "dpdk_cpp/dpdk_poolsetter_class.hpp"
#include "dpdk_cpp/dpdk_mempool_class.hpp"
#include "dpdk_cpp/dpdk_info_class.hpp"

#include "goose_parser.hpp"
#include "common/goose_container.hpp"
#include "common/utils.hpp"
#include "common/shared_defs.hpp"

#include "cxxopts.hpp"

#include <signal.h>
#include <atomic>
#include <stdexcept>

struct LCoreWorker
{
    rte_ring*   m_ring = nullptr;
    unsigned    m_lcore = 0;
    DPDK::CyclicStat m_workStat;

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
                gooseNum = result["goose"].as< int >();
            }
            if (result.count("sv80")) {
                sv80Num = result["sv80"].as< int >();
            }
            if (result.count("sv256")) {
                sv256Num = result["sv256"].as< int >();
            }
        } catch (const std::exception &e) {
            std::cerr << "cxxopts: Error parsing options: " << e.what() << std::endl;
            throw;
        }
    }

    void Init(int argc, char* argv[]) {
        ParseCmdOptions(argc, argv);

        for (unsigned i=0;i<gooseNum;++i) {
            GooseSource::ptr src = std::make_shared< GooseSource >();
            src->SetMAC(MAC("01:0C:CD:04:00:00"))
                .SetAppID(0x0001 + i)
                .SetGOID(std::format("GOID{:08}", i + 1))
                .SetDataSetRef(std::format("IED{:08}LDName/LLN0$DataSet", i + 1))
                .SetGOCBRef(std::format("IED{:08}LDName/LLN0$GO$GOCB", i + 1))
                .SetCRev(1)
                .SetNumEntries(16);
            gooseMap[src->GetPassport()] = src;

            std::cout << "Configured GOOSE[" << i + 1 << "]:\n"
                      << src->GetPassport()
                      << std::endl;
        }
    }

    inline void ProcessGoosePacket(const uint8_t *packet, size_t packetSize) {
        GoosePassport pass;
        GooseState state;

        int retval = parse_goose_packet(packet, packetSize, pass, state);
        if (retval < 0) {
            // Invalid GOOSE packet
            ++errGooseParserCnt;
        } else {
            auto src = gooseMap.find(pass);
            if (src != gooseMap.end()) {
                src->second->ProcessState(pass, state);

                ++rxGoosePacketCnt;
            } else {
                ++unknownGooseCnt;
                /*
                std::cout << "Received unknown GOOSE:\n"
                          << pass << state << std::endl;
                */
            }
        }
    }

public:
    // Settings
    unsigned        gooseNum = 0,
                    sv80Num = 0,
                    sv256Num = 0;

    // GOOSE
    GooseContainer  gooseMap;

    // Proto statistics
    uint64_t        nonGoosePacketCnt = 0,
                    rxGoosePacketCnt = 0,
                    errGooseParserCnt = 0,
                    unknownGooseCnt = 0;

    // Runtime
    std::atomic_bool doWork = true;
    std::vector< LCoreWorker > worker;
} g_app;

void signal_handler(int)
{
    g_app.doWork = false;
}

static void* stat_thread(void* args)
{
    #define BYTES_TO_MEGABITS(b)  ((b) * 8 / 1000000.0)
    #define BYTES_TO_MEGABYTES(b) ((b) / 1000000.0)

    unsigned port_id = 0, interval_sec = 2, current_sec = 0;

    while (g_app.doWork) {
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
                  << "\t\tTotal:    " << g_app.rxGoosePacketCnt << "\n"
                  << "\t\tError:    " << g_app.errGooseParserCnt << "\n"
                  << "\t\tUnknown:  " << g_app.unknownGooseCnt << "\n"
                  << "\t\tNonGOOSE: " << g_app.nonGoosePacketCnt
                  << std::endl;
    }
    return NULL;
}

static int lcore_processor(void *arg)
{
    LCoreWorker *conf = reinterpret_cast< LCoreWorker* >(arg);
    if (conf == nullptr) {
        g_app.doWork = false;
        std::cerr << "LCore: Config is NULL!" << std::endl;
        return -1;
    }

    set_thread_priority(DEF_WORKER_PRIORITY);

    const unsigned BURST_SIZE = 32;
    rte_mbuf *bufs[BURST_SIZE] = {};
    conf->m_workStat.MarkStartCycling();
    while (g_app.doWork) {
        uint16_t rxNum = rte_ring_dequeue_burst(conf->m_ring,
                                                (void **)bufs,
                                                BURST_SIZE,
                                                NULL);
        if (rxNum > 0) {
            conf->m_workStat.MarkProcBegin();
            for (uint16_t i=0;i<rxNum;++i) {
                const uint8_t *packet = rte_pktmbuf_mtod(bufs[i], const uint8_t *);
                const unsigned packetSize = rte_pktmbuf_pkt_len(bufs[i]);

                g_app.ProcessGoosePacket(packet, packetSize);
                /* rte_pktmbuf_free(bufs[i]); */
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
    const unsigned MBUF_NUM = 128 * 1024, CACHE_NUM = 64;
    DPDK::Mempool pool("bus_proc_pool", MBUF_NUM, CACHE_NUM);

    // Create Ethernet port
    const unsigned RX_DESC_NUM = 4 * 1024, TX_DESC_NUM = 1 * 1024;
    uint16_t port_id = 0, queue_id = 0;
    DPDK::Port eth = DPDK::PortBuilder(port_id)
                            .SetMemPool(pool.Get())
                            .AdjustQueues(1, 1)
                            .SetDescriptors(RX_DESC_NUM, TX_DESC_NUM)
                            .SetPromisc()
                            .Build();

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
    const unsigned WORKET_RING_SIZE = 4096;
    unsigned lcore = 0, wIndex = 0;
    g_app.worker.reserve(rte_lcore_count());
    RTE_LCORE_FOREACH_WORKER(lcore) {
        std::string ringName = "worker_ring_" + std::to_string(lcore);
        rte_ring *ring = rte_ring_create(ringName.c_str(),
                                         WORKET_RING_SIZE,
                                         rte_socket_id(),
                                         RING_F_SP_ENQ | RING_F_SC_DEQ);
        if (ring == nullptr) {
            throw std::runtime_error("Can't create ring for worker: " + ringName);
        }

        g_app.worker.push_back(LCoreWorker(ring, lcore));
        rte_eal_remote_launch(lcore_processor, &g_app.worker[wIndex], lcore);
        ++wIndex;
    }
    const unsigned workerNum = g_app.worker.size();

    // Start NIC port
    eth.Start();
    if (!eth.WaitLink(10)) {
        throw std::runtime_error("Link is still down after 10 sec...");
    }

    std::cout << "Start main loop with workers: " << workerNum << std::endl;

    const unsigned BURST_SIZE = 64;
    rte_mbuf* bufs[BURST_SIZE] = { 0 };

    DPDK::CyclicStat workStat;
    workStat.MarkStartCycling();
    while (g_app.doWork) {
        uint16_t rxNum = rte_eth_rx_burst(eth.GetID(), queue_id, bufs, BURST_SIZE);
        if (rxNum > 0) {
            workStat.MarkProcBegin();

            for (uint16_t i=0;i<rxNum;++i) {
                const uint8_t *packet = rte_pktmbuf_mtod(bufs[i], const uint8_t *);
                size_t packetSize = rte_pktmbuf_pkt_len(bufs[i]);

                uint16_t appid = 0;
                if (is_goose(packet, packetSize, &appid)) {
                    if (workerNum > 0) {
                        unsigned workerIdx = appid % workerNum;

                        if (rte_ring_enqueue(g_app.worker[workerIdx].m_ring, bufs[i]) == 0) {
                            continue;
                        }
                    }
                    
                    // Handling without lcores
                    g_app.ProcessGoosePacket(packet, packetSize);
                } else {
                    // TODO: Dispatch to: Thread - Kernel(TAP)
                    ++g_app.nonGoosePacketCnt;
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

    for (const auto &src : g_app.gooseMap) {
        std::cout << "GOOSE: \n" << *src.second << std::endl;
    }

    std::cout << "\nProcessing(main):\n" << workStat << std::endl;
    for (const auto &w : g_app.worker) {
        std::cout << "Worker: [" << w.m_lcore << "]\n"
                  << w.m_workStat
                  << std::endl;
    }
    std::cout << "Mempool: \n" << pool << std::endl;
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

