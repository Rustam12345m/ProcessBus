#pragma once

#include <rte_eal.h>
#include <rte_mempool.h>
#include <rte_ring.h>

#include <string>
#include <iostream>

namespace DPDK
{
    class Mempool
    {
    public:
        Mempool(const std::string& name, unsigned num_mbufs, unsigned mbuf_cache_size,
                unsigned mbuf_size = RTE_MBUF_DEFAULT_BUF_SIZE,
                int socket_id = rte_socket_id())
        {
            m_pool = rte_pktmbuf_pool_create(name.c_str(),
                                             num_mbufs,
                                             mbuf_cache_size,
                                             0,
                                             mbuf_size,
                                             socket_id);
            if (!m_pool) {
                throw std::runtime_error("Mempool creation failed: " + name);
            }
        }

        ~Mempool() {
            if (m_pool) {
                rte_mempool_free(m_pool);
                m_pool = nullptr;
            }
        }

        Mempool(const Mempool&) = delete;
        Mempool& operator=(const Mempool&) = delete;

        inline rte_mempool* Get() { return m_pool; }

        friend std::ostream& operator<<(std::ostream &out, const Mempool &obj) {
            out << "\tAvailable(mbuf): " << rte_mempool_avail_count(obj.m_pool) << "\n"
                << "\tInUse(mbuf):     " << rte_mempool_in_use_count(obj.m_pool) << "\n";
            return out;
        }

    private:
        rte_mempool* m_pool = nullptr;
    };
}

