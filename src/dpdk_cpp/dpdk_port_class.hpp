#pragma once

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_flow.h>

#include <stdexcept>
#include <vector>
#include <memory>
#include <iostream>

namespace DPDK
{
    /**
     * @class Port
     * @brief Representation of DPDK's Ethernet port
     */
    class Port
    {
        Port(uint16_t m_portID) : m_portID(m_portID) {}
    public:
        Port() = delete;
        Port(const Port&) = delete;
        Port& operator=(const Port&) = delete;

        ~Port() {
            if (rte_eth_dev_is_valid_port(m_portID)) {
                Stop();

                for (rte_flow *flow : m_flows) {
                    rte_flow_error error;
                    if (rte_flow_destroy(m_portID, flow, &error) < 0) {
                        std::cerr << std::string(error.message) << std::endl;
                    }
                }

                rte_eth_dev_close(m_portID);
                m_portID = 0xFFFF;
            }
        }

        void Start() {
            if (rte_eth_dev_start(m_portID) != 0) {
                throw std::runtime_error("Can't start port: " + std::to_string(m_portID));
            }
            m_isStarted = true;
        }
        void Stop() {
            if (m_isStarted) {
                rte_eth_dev_stop(m_portID);
                m_isStarted = false;
            }
        }

        inline uint16_t GetID() const { return m_portID; }

        void SetPromisc(bool enable = true) {
            if (enable) {
                rte_eth_promiscuous_enable(m_portID);
            } else {
                rte_eth_promiscuous_disable(m_portID);
            }
        }

        void SetAllMulticast(bool enable = true) {
            if (enable) {
                rte_eth_allmulticast_enable(m_portID);
            } else {
                rte_eth_allmulticast_disable(m_portID);
            }
        }

        bool WaitLink(unsigned sec) {
            rte_eth_link link;
            for (unsigned i=0;i<sec;++i) {
                int retval = rte_eth_link_get_nowait(m_portID, &link);
                if (retval < 0) {
                    throw std::runtime_error("Failed to get link status: "
                                             + std::string(rte_strerror(-retval)));
                } else if (link.link_status) {
                    break;
                }

                sleep(1);
            }
            return (link.link_status != 0);
        }

        void AddVLAN_Flow(uint16_t vlan_id, uint16_t queue_id) {
            rte_flow_attr attr = { .ingress = 1 };

            rte_flow_item_eth eth_spec = { .type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN) };
            rte_flow_item_eth eth_mask = { .type = 0xFFFF };

            rte_flow_item_vlan vlan_spec = { .tci = rte_cpu_to_be_16(vlan_id) };
            rte_flow_item_vlan vlan_mask = { .tci = 0xFFFF };

            rte_flow_item pattern[] = {
                { RTE_FLOW_ITEM_TYPE_ETH, &eth_spec, &eth_mask },
                { RTE_FLOW_ITEM_TYPE_VLAN, &vlan_spec, &vlan_mask },
                { RTE_FLOW_ITEM_TYPE_END, nullptr, nullptr },
            };

            rte_flow_action_queue queue = { .index = queue_id };
            rte_flow_action actions[] = {
                { RTE_FLOW_ACTION_TYPE_QUEUE, &queue },
                { RTE_FLOW_ACTION_TYPE_END, nullptr },
            };

            rte_flow_error error = {};
            if (rte_flow_validate(m_portID, &attr, pattern, actions, &error) != 0) {
                throw std::runtime_error("VLAN flow rule is invalid: " + std::string(error.message));
            }

            rte_flow* flow = rte_flow_create(m_portID, &attr, pattern, actions, &error);
            if (flow != nullptr) {
                m_flows.push_back(flow);
            } else {
                throw std::runtime_error("Can't create VLAN flow: " + std::string(error.message));
            }
        }

        void AddEthTypeFlow(uint16_t eth_type, uint16_t queue_id) {
            rte_flow_attr attr = { .priority=1, .ingress=1 };

            rte_flow_item_eth eth_spec = {
                .type = rte_cpu_to_be_16(eth_type),  // GOOSE | SV
            };
            rte_flow_item pattern[] = {
                { RTE_FLOW_ITEM_TYPE_ETH },
                { RTE_FLOW_ITEM_TYPE_VLAN, NULL },
                { RTE_FLOW_ITEM_TYPE_ETH, &eth_spec },
                { RTE_FLOW_ITEM_TYPE_END }
            };

            rte_flow_action_queue queue = { .index = queue_id };
            rte_flow_action actions[] = {
                { .type = RTE_FLOW_ACTION_TYPE_QUEUE, .conf = &queue },
                RTE_FLOW_ACTION_TYPE_END,
            };

            rte_flow_error error = {};
            if (rte_flow_validate(m_portID, &attr, pattern, actions, &error) != 0) {
                throw std::runtime_error("EthType flow rute is invalid: " + std::string(error.message));
            }

            rte_flow* flow = rte_flow_create(m_portID, &attr, pattern, actions, &error);
            if (flow != nullptr) {
                m_flows.push_back(flow);
            } else {
                throw std::runtime_error("Can't create Proto flow: " + std::string(error.message));
            }
        }

        friend std::ostream& operator<<(std::ostream &out, Port &obj) {
            rte_eth_dev_info devInfo = {};
            if (rte_eth_dev_info_get(obj.m_portID, &devInfo) == 0) {
                out << "\tMaxRxQueue = " << devInfo.max_rx_queues << "\n"
                    << "\tMaxDescNum = " << devInfo.rx_desc_lim.nb_max << "\n";
            }
            return out;
        }

    private:
        uint16_t m_portID = 0xFFFF;
        bool     m_isStarted = false;
        std::vector< rte_flow* > m_flows;

    friend class PortBuilder;
    };

    /**
     * @class PortBuilder
     * @brief Create a new DPDK port 
     */
    class PortBuilder
    {
    public:
        PortBuilder(uint16_t m_portID) : m_portID(m_portID) { }

        PortBuilder& SetMemPool(rte_mempool* pool) {
            m_mbufPool = pool;
            return *this;
        }

        PortBuilder& AdjustQueues(uint16_t rx, uint16_t tx) {
            m_rxQueueNum = rx;
            m_txQueueNum = tx;
            return *this;
        }

        PortBuilder& SetDescriptors(uint16_t rx, uint16_t tx) {
            m_rxDescNum = rx;
            m_txDescNum = tx;
            return *this;
        }

        Port Build() {
            if (m_mbufPool == nullptr) {
                throw std::runtime_error("Mempool is not set!");
            }

            if (!rte_eth_dev_is_valid_port(m_portID)) {
                throw std::runtime_error("Port ID is not valid: " + std::to_string(m_portID));
            }

            rte_eth_dev_info devInfo = {};
            if (rte_eth_dev_info_get(m_portID, &devInfo) != 0) {
                throw std::runtime_error("Can't get dev info for: " + std::to_string(m_portID));
            }

            if (m_timestamping) {
                if (devInfo.rx_offload_capa & RTE_ETH_RX_OFFLOAD_TIMESTAMP) {
                    m_ethConf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_TIMESTAMP;
                }
                if (devInfo.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE) {
                    m_ethConf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
                }
                /* Force full Tx path in the driver, required for IEEE1588 */
                m_ethConf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_MULTI_SEGS;
            }

            /* m_ethConf.link_speeds = RTE_ETH_LINK_SPEED_2_5G; */
            /* m_ethConf.link_speeds = RTE_ETH_LINK_SPEED_10G; */
            m_ethConf.link_speeds = RTE_ETH_LINK_SPEED_AUTONEG;

            if (rte_eth_dev_configure(m_portID,
                                      m_rxQueueNum,
                                      m_txQueueNum,
                                      &m_ethConf) != 0) {
                throw std::runtime_error("Can't configure port: " + std::to_string(m_portID));
            }

            if (rte_eth_dev_adjust_nb_rx_tx_desc(m_portID,
                                                 &m_rxDescNum,
                                                 &m_txDescNum) != 0) {
                throw std::runtime_error("Can't adjust RX/TX descriptors");
            }

            for (uint16_t q=0;q<m_rxQueueNum;++q) {
                rte_eth_rxconf *rxConf = &devInfo.default_rxconf;
                rxConf->offloads = m_ethConf.rxmode.offloads;

                if (rte_eth_rx_queue_setup(m_portID,
                                           q,
                                           m_rxDescNum,
                                           rte_socket_id(),
                                           rxConf,
                                           m_mbufPool) != 0) {
                    throw std::runtime_error("RX queue setup failed");
                }
            }

            for (uint16_t q=0;q<m_txQueueNum;++q) {
                rte_eth_txconf *txConf = &devInfo.default_txconf;
                txConf->offloads = m_ethConf.txmode.offloads;

                if (rte_eth_tx_queue_setup(m_portID,
                                           q,
                                           m_txDescNum,
                                           rte_socket_id(),
                                           txConf) != 0) {
                    throw std::runtime_error("TX queue setup failed");
                }
            }

            if (m_timestamping) {
                rte_eth_timesync_enable(m_portID);
            }
            return Port(m_portID);
        }

    public:
        uint16_t        m_portID = 0xFFFF;
        rte_eth_conf    m_ethConf = {};
        rte_mempool*    m_mbufPool = nullptr;
        uint16_t        m_rxQueueNum = 0,
                        m_txQueueNum = 0;
        uint16_t        m_rxDescNum = 1024,
                        m_txDescNum = 1024;
        bool            m_timestamping = false;
    };
}

