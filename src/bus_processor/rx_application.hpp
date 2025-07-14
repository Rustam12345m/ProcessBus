#pragma once

#include "common/utils.hpp"
#include "common/shared_defs.hpp"
#include "common/goose_container.hpp"
#include "common/sv_container.hpp"

#include "dpdk_cpp/dpdk_cyclestat_class.hpp"
#include "dpdk_cpp/dpdk_port_class.hpp"

#include <vector>
#include <memory>

using StopVarType = volatile bool;

enum BUS_PROTO
{
    NON_BUS_PROTO = 0,
    BUS_PROTO_SV,
    BUS_PROTO_GOOSE,
    /* BUS_PROTO_PTP */
};

class RX_Application;

/**
 * @brief Pipeline handler on each logical core in DPDK
 */
struct PipelineProcessor
{
    rte_ring*           m_ring = nullptr;
    RX_Application*     m_app = nullptr;
    unsigned            m_lcore = 0;
    uint64_t            m_noFreeDesc = 0;
    DPDK::CyclicStat    m_workStat;

    PipelineProcessor(rte_ring *ring, RX_Application *app, unsigned lcore)
        : m_ring(ring), m_app(app), m_lcore(lcore)
    {}
};

/**
 * @brief RX and process packets from ProcessBus
 */
class RX_Application
{
public:
    using ptr = std::shared_ptr< RX_Application >;
    RX_Application(int argc, char *argv[]);

    void DisplayStatistic(unsigned interval_sec);

    void Run(StopVarType &doWork);

private:
    void ParseCmdOptions(int argc, char* argv[]);
    void Init(int argc, char* argv[]);

public:
/* private */
    // Settings
    unsigned        m_confGooseNum = 0,
                    m_confSV80Num = 0,
                    m_confSV256Num = 0;

    // Runtime
    GooseContainer  m_gooseMap;
    SVContainer     m_svMap;

    // Statistic
    uint64_t        m_rxGoosePktCnt = 0, m_rxSVPktCnt = 0,
                    m_errGooseParserCnt = 0, m_errSVParserCnt = 0,
                    m_rxUnknownGooseCnt = 0, m_rxUnknownSVCnt = 0,
                    m_pktToKernelCnt = 0;
    rte_eth_stats   m_lastPortStat = {};
    unsigned        m_statDisplaySec = 0;
};

