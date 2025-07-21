#pragma once

#include "pipeline.hpp"
#include "process_bus_parser.hpp"

#include <rte_mbuf.h>
#include <rte_prefetch.h>

constexpr unsigned  RX_BURST_SIZE = 32;

namespace PBus
{
    /*
        Frame processing:
        {mbuf} -> RouterStage -> GooseStage -> SampledValuesStage -> IPStage
    */

    enum EnumStages
    {
        START_STAGE = 0,

        ROUTER = 0,
        GOOSE,
        SV,
        IP,

        STAGE_NUM
    };

    template< typename TMatrix, unsigned TFrameIdx >
    struct RouterStage
    {
        static void ApplyTo(TMatrix& matrix) {
            typename TMatrix::Frame &frame = matrix.stages[TFrameIdx];

            for (unsigned i=0;i<frame.num;++i) {
                /* rte_prefetch0(frame.buf[i]); */
                const uint8_t *packet = rte_pktmbuf_mtod(frame.buf[i], const uint8_t *);

                unsigned appid = 0;
                BUS_PROTO type = ProcessBusParser::get_proto_type(packet, &appid);
                switch (type) {
                case BUS_PROTO_SV: {
                    matrix.stages[SV].PutBuffer(frame.buf[i]);
                    break;
                }
                case BUS_PROTO_GOOSE: {
                    matrix.stages[GOOSE].PutBuffer(frame.buf[i]);
                    break;
                }
                default: {
                    matrix.stages[IP].PutBuffer(frame.buf[i]);
                    break;
                }
                }
            }

            //! Clean frame for the next cycle
            frame.num = 0;
        }
    };

    template< typename TMatrix, unsigned TFrameIdx >
    struct GooseStage
    {
        static void ApplyTo(TMatrix &matrix) {
            typename TMatrix::Frame &frame = matrix.stages[TFrameIdx];

            RX_Application &app = *matrix.app;
            for (unsigned i=0;i<frame.num;++i) {
                const uint8_t *packet = rte_pktmbuf_mtod(frame.buf[i], const uint8_t *);
                const unsigned size = rte_pktmbuf_pkt_len(frame.buf[i]);
                
                GoosePassport pass;
                GooseState state;
                int retval = ProcessBusParser::parse_goose_packet(packet, size, pass, state);
                if (retval == 0) {
                    auto src = app.m_gooseMap.find(pass);
                    if (src != app.m_gooseMap.end()) {
                        src->second->ProcessState(pass, state);

                        ++app.m_rxGoosePktCnt;
                    } else {
                        ++app.m_rxUnknownGooseCnt;
                    }
                } else {
                    // Invalid GOOSE packet
                    ++app.m_errGooseParserCnt;
                }
            }

            //! Clean frame for the next cycle
            frame.num = 0;
        }
    };

    template< typename TMatrix, unsigned TFrameIdx >
    struct SampledValuesStage
    {
        static void ApplyTo(TMatrix &matrix) {
            typename TMatrix::Frame &frame = matrix.stages[TFrameIdx];

            RX_Application &app = *matrix.app;
            for (unsigned i=0;i<frame.num;++i) {
                const uint8_t *packet = rte_pktmbuf_mtod(frame.buf[i], const uint8_t *);
                const unsigned size = rte_pktmbuf_pkt_len(frame.buf[i]);

                SVStreamPassport pass;
                SVStreamState state;
                int retval = ProcessBusParser::parse_sv_packet(packet, size, pass, state);
                if (retval == 0) {
                    auto src = app.m_svMap.find(pass);
                    if (src != app.m_svMap.end()) {
                        src->second->ProcessState(pass, state);

                        ++app.m_rxSVPktCnt;
                    } else {
                        ++app.m_rxUnknownSVCnt;
                    }
                } else {
                    ++app.m_errSVParserCnt;
                }
            }

            //! Clean frame for the next cycle
            frame.num = 0;
        }
    };

    template< typename TMatrix, unsigned TFrameIdx >
    struct IPStage 
    {
        static void ApplyTo(TMatrix &matrix) {
            typename TMatrix::Frame &frame = matrix.stages[TFrameIdx];

            matrix.app->m_pktToKernelCnt += frame.num;

            //! Clean frame for the next cycle
            frame.num = 0;
        }
    };

    /**
     * @brief DataMatrix represents a pipeline's table:
     * { mbuf } x { stages } where each stage has its own frame which is an array of mbufs
     */
    using DataMatrix = Pipeline::Matrix< EnumStages,
                                         Pipeline::Frame< rte_mbuf, RX_BURST_SIZE >,
                                         RX_Application >;

    using FramePipeline = Pipeline::StaticChain< RouterStage< DataMatrix, ROUTER >,
                                                 GooseStage< DataMatrix, GOOSE >,
                                                 SampledValuesStage< DataMatrix, SV >,
                                                 IPStage< DataMatrix, IP > >;
}

