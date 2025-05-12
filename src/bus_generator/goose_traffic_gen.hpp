#pragma once

#include "tx_unit.hpp"
#include "common/goose_container.hpp"
#include "rte_byteorder.h"

#include <cstdint>
#include <cstring>

#define MAX_GOOSE_PACKET_SIZE      1518

enum GOOSE_PARAM_OFFSETS
{
    GOOSE_APPID_OFFSET = 0,
    GOOSE_TIMESTAMP_OFFSET,
    GOOSE_GOID_OFFSET,
    GOOSE_GOCB_REF_OFFSET,
    GOOSE_DS_REF_OFFSET,
    GOOSE_SQ_NUM_OFFSET,
    GOOSE_ST_NUM_OFFSET,
    GOOSE_D1_OFFSET,

    GOOSE_OFFSET_NUM
};

struct GooseSourceIED
{
    char        sID[8 + 1] = { 0 }; // Is used in Patterns: GOID, GOCB_REF, DS_REF
    uint32_t    stNum = 0;
    uint32_t    sqNum = 0;
    uint64_t    timestamp = 0;
};

struct GoosePacketDesc
{
    unsigned    idx = 0; // The GOOSE IED index
};

/**
 * @brief CreateUnitser a banch of Gooses with fixed offsets
 */
class GooseTrafficGen
{
public:
    using Desc = GoosePacketDesc;
    using TxGooseUnit = TxUnit< GoosePacketDesc >;
    using TxUnitArray = std::vector< GooseTrafficGen::TxGooseUnit >;
    GooseTrafficGen(unsigned MaxGooseNum, unsigned SndFreq, unsigned SignalsPerGoose);

    inline size_t AmendPacket(uint8_t *packet, const GoosePacketDesc &desc)
    {
        GooseSourceIED &ied = m_ieds[desc.idx];

        *(uint16_t *)(packet + m_offsets[GOOSE_APPID_OFFSET]) = RTE_STATIC_BSWAP16((desc.idx + 1) & 0xFFFF);
        *(uint64_t *)(packet + m_offsets[GOOSE_GOID_OFFSET] + 4/*GOID*/) = *(uint64_t *)ied.sID;
        *(uint64_t *)(packet + m_offsets[GOOSE_GOCB_REF_OFFSET] + 3/*IED*/) = *(uint64_t *)ied.sID;
        *(uint64_t *)(packet + m_offsets[GOOSE_DS_REF_OFFSET] + 3/*IED*/) = *(uint64_t *)ied.sID;

        ied.timestamp += m_tsDeltaChange;
        *(uint64_t *)(packet + m_offsets[GOOSE_TIMESTAMP_OFFSET]) = ied.timestamp;

        ++ied.stNum;
        *(uint32_t *)(packet + m_offsets[GOOSE_ST_NUM_OFFSET]) = RTE_STATIC_BSWAP32(ied.stNum);
        *(uint32_t *)(packet + m_offsets[GOOSE_SQ_NUM_OFFSET]) = RTE_STATIC_BSWAP32(ied.sqNum);

        packet[m_offsets[GOOSE_D1_OFFSET]] = (ied.stNum % 2 == 0) ? 1 : 0;

        return m_skeletonSize;
    }

    size_t      GetSkeletonSize() const { return m_skeletonSize; }
    uint8_t*    GetSkeletonBuffer() { return m_skeleton; }

    GooseTrafficGen::TxUnitArray& GetTxUnits() { return m_units; }   

private:
    void        InitIED(GooseSourceIED &ied, unsigned idx);
    void        MakeSkeletonPacket(unsigned sigNum);
    void        GenerateTxUnits(unsigned pps);

private:
    std::vector< GooseSourceIED >   m_ieds; // 1 GOOSE <-> 1 IED
    GooseTrafficGen::TxUnitArray    m_units; // TX moments with {blocks}

    unsigned    m_signalNum = 16;
    unsigned    m_tsDeltaChange = 0;

    uint16_t    m_offsets[GOOSE_OFFSET_NUM] = { 0 };
    uint8_t     m_skeleton[MAX_GOOSE_PACKET_SIZE] = { 0 };
    size_t      m_skeletonSize = 0;
};

