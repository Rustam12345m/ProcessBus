#pragma once

#include "common/utils.hpp"
#include "dpdk_cpp/dpdk_cyclestat_class.hpp"

using StopVarType = volatile bool;

struct GenAppStat
{
    DPDK::CyclicStat procStat;
    unsigned         errSendCnt = 0;
};

/**
 * @brief Main logic for bus generator app
 */
class GenApplication
{
public:
    GenApplication(int argc, char *argv[]);

    void DisplayStatistic();

    void Run(StopVarType &doWork);

private:
    // Config
    unsigned m_gooseNum = 0,
             m_gooseSendFreq = 1,
             m_sv80Num = 0,
             m_sv256Num = 0;

    // Statistics
    GenAppStat m_stat;
};

