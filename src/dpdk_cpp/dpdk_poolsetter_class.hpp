#pragma once

#include <rte_mempool.h>
#include <rte_mbuf.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace DPDK
{
    class PoolSetter
    {
    public:
        PoolSetter(const uint8_t *skeleton, size_t skeletonSize)
            : m_skeleton(skeleton, skeleton + skeletonSize)
        {}

        void FillPackets(struct rte_mempool *mp) {
            rte_mempool_obj_iter(mp, fill_mbuf_callback, this);
        }

        static void fill_mbuf_callback(struct rte_mempool *mp, void *arg,
                                       void *obj, unsigned idx)
        {
            auto *instance = static_cast< PoolSetter *>(arg);
            instance->ProcessMbuf(static_cast<struct rte_mbuf *>(obj), idx);
        }

    private:
        void ProcessMbuf(struct rte_mbuf *mbuf, unsigned idx) {
            uint8_t *data = rte_pktmbuf_mtod(mbuf, uint8_t *);
            std::memcpy(data, m_skeleton.data(), m_skeleton.size());
            mbuf->data_len = m_skeleton.size();
            mbuf->pkt_len = mbuf->data_len;

            rte_pktmbuf_free(mbuf);
        }

    private:
        std::vector< uint8_t > m_skeleton;
    };
}

