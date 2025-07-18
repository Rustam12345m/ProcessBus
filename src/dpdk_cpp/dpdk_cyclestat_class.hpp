#pragma once

#include "dpdk_clocks_class.hpp"

namespace DPDK
{
    class CyclicStat
    {
    public:
        CyclicStat() {
            m_minProcessByTicks = DPDK::Clocks::get_ticks_per_sec();
        }

        inline uint64_t GetStartTick() const {
            return m_startCycling;
        }

        inline void MarkStartCycling() {
            m_startCycling = DPDK::Clocks::get_current_ticks();
        }
        inline void MarkFinishCycling() {
            uint64_t finishTick = DPDK::Clocks::get_current_ticks();
            m_loadPerc = (double)m_totalProcessTicks / (finishTick - m_startCycling) * 100.0;
            m_waitPerc = (100.0 - m_loadPerc);
            m_maxProcUS = DPDK::Clocks::ticks_to_us(m_maxProcessByTicks);
            m_minProcUS = DPDK::Clocks::ticks_to_us(m_minProcessByTicks);
        }

        inline void MarkProcBegin() {
            m_procBegin = DPDK::Clocks::get_current_ticks();
        }
        inline void MarkProcEnd() {
            uint64_t delta = DPDK::Clocks::get_current_ticks() - m_procBegin;
            if (delta > m_maxProcessByTicks) {
                m_maxProcessByTicks = delta;
            }
            if (delta < m_minProcessByTicks) {
                m_minProcessByTicks = delta;
            }
            m_totalProcessTicks += delta;
        }

        inline double GetLoadPerc() const {
            return m_loadPerc;
        }
        inline double GetWaitPerc() const {
            return m_waitPerc;
        }
        inline unsigned GetMaxProcUS() const {
            return m_maxProcUS;
        }
        inline unsigned GetMinProcUS() const {
            return m_minProcUS;
        }

    private:
        uint64_t    m_startCycling = 0, m_procBegin = 0;
        uint64_t    m_totalProcessTicks = 0,
                    m_maxProcessByTicks = 0,
                    m_minProcessByTicks = 0;

        double      m_loadPerc = 0.0, m_waitPerc = 0.0;
        unsigned    m_maxProcUS = 0, m_minProcUS = 0;
    };
}

