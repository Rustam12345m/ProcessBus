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

/**
 *
 */
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
} g_app;

void signal_handler(int)
{
    g_app.doWork = false;
}

static void* stat_thread(void* args)
{
    #define BYTES_TO_MEGABITS(b)  ((b) * 8 / 1000000.0)
    #define BYTES_TO_MEGABYTES(b) ((b) / 1000000.0)

    unsigned port_id = 0, interval_sec = 1, current_sec = 0;

    while (g_app.doWork) {
        rte_eth_stats stats_start, stats_end;

        rte_eth_stats_get(port_id, &stats_start);
        sleep(interval_sec); // Wait
        ++current_sec;
        rte_eth_stats_get(port_id, &stats_end);

        // Calculate RX and TX PPS/BPS
        uint64_t rx_pps = (stats_end.ipackets - stats_start.ipackets) / interval_sec;
        uint64_t tx_pps = (stats_end.opackets - stats_start.opackets) / interval_sec;

        uint64_t rx_bps = (stats_end.ibytes - stats_start.ibytes) / interval_sec;
        uint64_t tx_bps = (stats_end.obytes - stats_start.obytes) / interval_sec;

        printf("\r\nStatistics: Port %d (interval: %d sec, current: %d sec):\n",
               port_id, interval_sec, current_sec);

        printf("\tRX-PPS: %" PRIu64 ", RX-BPS: %" PRIu64 " (%.2f Mbps, %.2f MBps)\n",
                rx_pps, rx_bps,
                BYTES_TO_MEGABITS(rx_bps),
                BYTES_TO_MEGABYTES(rx_bps));

        printf("\tTX-PPS: %" PRIu64 ", TX-BPS: %" PRIu64 " (%.2f Mbps, %.2f MBps)\n",
                tx_pps, tx_bps,
                BYTES_TO_MEGABITS(tx_bps),
                BYTES_TO_MEGABYTES(tx_bps));

        std::cout << "\tGOOSE RX:\n"
                  << "\t\tRX: " << g_app.rxGoosePacketCnt << "\n"
                  << "\t\tError: " << g_app.errGooseParserCnt << "\n"
                  << "\t\tUnknown: " << g_app.unknownGooseCnt << "\n"
                  << "\t\tNonGOOSE: " << g_app.nonGoosePacketCnt
                  << std::endl;

        rte_eth_stats stats;
        if (rte_eth_stats_get(port_id, &stats) == 0) {
            printf("\tRX-errors:  %" PRIu64 "\n", stats.ierrors);
            printf("\tTX-errors:  %" PRIu64 "\n", stats.oerrors);
            printf("\tRX-missed:  %" PRIu64 "\n", stats.imissed);
            printf("\tRX-no-mbuf: %" PRIu64 "\n", stats.rx_nombuf);
        }
    }
    return NULL;
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
    DPDK::Info::display_lcore_info();
    DPDK::Info::display_eth_info();
    DPDK::Info::display_pools_info();

    // Statistics
    pthread_t statThHandle;
    pthread_create(&statThHandle, NULL, stat_thread, NULL);

    // Pin to CPU & RT priority
    pin_thread_to_cpu(DEF_BUS_RX_CPU, DEF_PROCESS_PRIORITY);

    // Start NIC port
    eth.Start();

    std::cout << "Start main loop\n";
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
                    // TODO: Dispatch to lcores or process here
                    g_app.ProcessGoosePacket(packet, packetSize);
                } else {
                    // TODO: Dispatch to: Thread - Kernel(TAP)
                    ++g_app.nonGoosePacketCnt;
                }

                // Free the received mb
                rte_pktmbuf_free(bufs[i]);
            }

            workStat.MarkProcEnd();
        }
    }
    workStat.MarkFinishCycling();

    eth.Stop();
    pthread_join(statThHandle, nullptr);

    for (const auto &src : g_app.gooseMap) {
        std::cout << "GOOSE: \n" << *src.second << std::endl;
    }
    std::cout << "\nProcessing statistics:\n" << workStat << std::endl;
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

    if (rte_eth_dev_count_avail()== 0) {
        rte_exit(EXIT_FAILURE, "No available ports. Please, check port binding.\n");
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

