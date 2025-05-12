#pragma once

#include <rte_cycles.h>
#include <rte_timer.h>
#include <cstdint>

namespace DPDK
{
    class Clocks
    {
    public:
        // Convert microseconds to CPU ticks
        static inline uint64_t us_to_ticks(uint64_t us) {
            uint64_t ticks_per_second = rte_get_timer_hz();
            return (us * ticks_per_second) / 1000000ULL;
        }
        static inline uint64_t ticks_to_us(uint64_t ticks) {
            return (ticks * 1'000'000ULL) / rte_get_timer_hz();
        }
        static inline uint64_t get_ticks_per_sec() {
            return rte_get_timer_hz();
        }

        // Get current CPU ticks
        static inline uint64_t get_current_ticks() {
            return rte_get_timer_cycles();
        }

        static inline uint64_t delay_us_to_ticks(uint64_t delay_us) {
            uint64_t ticks_per_second = rte_get_timer_hz();
            return (delay_us * ticks_per_second) / 1000000ULL;
        }

        // Delay for a specified number of microseconds
        static void delay_us(uint64_t us) {
            uint64_t ticks_per_second = rte_get_timer_hz();
            uint64_t delay = (us * ticks_per_second) / 1000000ULL;
            delay_ticks(delay);
        }

        // Delay for a specified number of CPU ticks
        static inline void delay_ticks(uint64_t delay) {
            uint64_t start_ticks = rte_get_timer_cycles();
            while (rte_get_timer_cycles() - start_ticks < delay) {
                rte_pause();
            }
        }

        static inline uint64_t delay_until_ticks(uint64_t target_ticks) {
            uint64_t ticks = 0;
            while ((ticks = rte_get_timer_cycles()) < target_ticks) {
                rte_pause();
            }
            return ticks;
        }
    };
}

