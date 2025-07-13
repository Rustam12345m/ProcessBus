#pragma once

#include "common/utils.hpp"
#include "common/shared_defs.hpp"

using StopVarType = volatile bool;

/**
 * @brief Main logic for bus generator app
 */
class GenApplication
{
public:
    GenApplication(int argc, char *argv[]);

    void run(StopVarType &doWork);

private:
    unsigned m_gooseNum = 0,
             m_gooseSendFreq = 1,
             m_sv80Num = 0,
             m_sv256Num = 0;

    // DPDK settings
    const unsigned MBUF_NUM = 64 * 1024,
                   CACHE_NUM = 64;
    const unsigned RX_DESC_NUM = 1 * 1024,
                   TX_DESC_NUM = 8 * 1024;
};

