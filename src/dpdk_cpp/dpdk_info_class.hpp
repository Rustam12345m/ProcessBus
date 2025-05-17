#pragma once

#include <rte_eal.h>
#include <rte_version.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_mempool.h>

#include <iomanip>
#include <iostream>

namespace DPDK
{
    class Info
    {
    public:
        static void display_common_info()
        {
            std::cout << "DPDK version: " << std::string(rte_version()) << "\n"
                      << "Available ports:" << rte_eth_dev_count_avail() << "\n"
                      << "LCore: " << rte_lcore_count()
                      << std::endl;
        }

        static void display_lcore_info()
        {
            std::cout << std::left
                      << std::setw(10) << "Lcore ID"
                      << std::setw(15) << "CPU ID"
                      << std::setw(15) << "Socket ID"
                      << "\n";
            std::cout << std::string(40, '-') << "\n";

            unsigned lcore_id = 0;
            RTE_LCORE_FOREACH(lcore_id) {
                if (rte_lcore_is_enabled(lcore_id)) {
                    std::cout << std::setw(10) << lcore_id 
                              << std::setw(15) << rte_lcore_to_cpu_id(lcore_id)
                              << std::setw(15) << rte_lcore_to_socket_id(lcore_id)
                              << "\n";
                }
            }
            std::cout << std::endl;
        }

        static void display_eth_info()
        {
            std::cout << std::setw(10) << "Port ID"
                      << std::setw(20) << "Driver Name" 
                      << std::setw(15) << "RX Queues"
                      << std::setw(15) << "TX Queues" 
                      << std::setw(20) << "MAC Address"
                      << "\n";
            std::cout << std::string(80, '-') << "\n";

            uint16_t port_id = 0;
            RTE_ETH_FOREACH_DEV(port_id) {
                rte_eth_dev_info dev;
                rte_eth_dev_info_get(port_id, &dev);

                rte_ether_addr mac;
                rte_eth_macaddr_get(port_id, &mac);

                char mac_str[18];
                snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                         mac.addr_bytes[0], mac.addr_bytes[1], mac.addr_bytes[2],
                         mac.addr_bytes[3], mac.addr_bytes[4], mac.addr_bytes[5]);
                std::cout << std::setw(10) << port_id 
                          << std::setw(20) << dev.driver_name
                          << std::setw(15) << dev.nb_rx_queues
                          << std::setw(15) << dev.nb_tx_queues
                          << std::setw(20) << mac_str
                          << "\n";
            }
            std::cout << std::endl;
        }

        static void display_eth_stats(uint16_t port_id)
        {
            rte_eth_stats stats;
            if (rte_eth_stats_get(port_id, &stats) != 0) {
                printf("Error getting port statistics for port %u\n", port_id);
                return;
            }

            printf("Port %u Statistics:\n", port_id);
            printf("\tRX-packets: %" PRIu64 "\n", stats.ipackets);
            printf("\tTX-packets: %" PRIu64 "\n", stats.opackets);
            printf("\tRX-bytes:   %" PRIu64 "\n", stats.ibytes);
            printf("\tTX-bytes:   %" PRIu64 "\n", stats.obytes);
            printf("\tRX-errors:  %" PRIu64 "\n", stats.ierrors);
            printf("\tTX-errors:  %" PRIu64 "\n", stats.oerrors);
            printf("\tRX-missed:  %" PRIu64 "\n", stats.imissed);
            printf("\tRX-no-mbuf: %" PRIu64 "\n", stats.rx_nombuf);

            int nb_xstats = rte_eth_xstats_get(port_id, NULL, 0); // find how many
            rte_eth_xstat xstats[nb_xstats];
            rte_eth_xstats_get(port_id, xstats, nb_xstats);

            rte_eth_xstat_name names[nb_xstats];
            rte_eth_xstats_get_names(port_id, names, nb_xstats);
            for (int i = 0; i < nb_xstats; i++) {
                printf("\t%s, Value: %" PRIu64 "\n", names[i].name, xstats[i].value);
            }
        }

        static void display_link_status(uint16_t port_id)
        {
            rte_eth_link link;
            rte_eth_link_get_nowait(port_id, &link);

            if (link.link_status) {
                printf("Port %u Link Up - Speed: %u Mbps - %s\n",
                       port_id,
                       link.link_speed,
                       link.link_duplex == RTE_ETH_LINK_FULL_DUPLEX ? "Full Duplex" : "Half Duplex");
            } else {
                printf("Port %u Link Down\n", port_id);
            }
        }

        static void display_pools_info()
        {
            std::cout << std::setw(20) << "Name"
                      << std::setw(15) << "Size" 
                      << std::setw(15) << "Avail Count"
                      << std::setw(15) << "Cache Size"
                      << "\n";

            std::cout << std::string(65, '-') << "\n";
            rte_mempool_walk([](struct rte_mempool *mp, void *) {
                std::cout << std::setw(20) << mp->name 
                          << std::setw(15) << rte_mempool_avail_count(mp) + rte_mempool_in_use_count(mp)
                          << std::setw(15) << rte_mempool_avail_count(mp)
                          << std::setw(15) << mp->cache_size << "\n";
            }, NULL);
            std::cout << std::endl;
        }

        static void display_mbuf_info(const rte_mbuf *mbuf)
        {
            printf("MBuf Information:\n");
            printf("\tPacket Length: %u\n", rte_pktmbuf_pkt_len(mbuf));
            printf("\tData Length: %u\n", rte_pktmbuf_data_len(mbuf));
            printf("\tPort ID: %u\n", mbuf->port);
            printf("\tRSS Hash: 0x%X\n", mbuf->hash.rss);
            printf("\tVLAN TCI: 0x%X\n", mbuf->vlan_tci);
            printf("\tOffload Flags: 0x%lX\n", mbuf->ol_flags);
            /* printf("  Timestamp: %" PRIu64 "\n", mbuf->timestamp); */

            // Print raw packet data
            printf("\nPacket Data:\n");
            const uint8_t *data = rte_pktmbuf_mtod(mbuf, const uint8_t *);
            for (uint32_t i = 0; i < rte_pktmbuf_data_len(mbuf); i++) {
                printf("%02X ", data[i]);
                if ((i + 1) % 16 == 0) {
                    printf("\n");
                }
            }
            printf("\n");
        }
    };
}

