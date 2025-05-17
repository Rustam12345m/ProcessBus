#pragma once

#include "tx_unit.hpp"
#include "rte_byteorder.h"

#define MAX_SV_PACKET_SIZE      1518
#define MAX_SV_ASDU_NUM         8

enum SV_ASDU_OFFSETS
{
    SV_SVID_OFFSET = 0,
    SV_SMP_CNT_OFFSET,
    SV_DATA_OFFSET,

    SV_ASDU_OFFSET_NUM
};

struct SVSourceIED
{
    char sID[4 + 1] = { 0 }; // Is used in Patterns

    uint16_t smpCnt = 0;
};

struct SVPacketDesc
{
    unsigned idx = 0; // The IED index
};

enum SV_TYPE
{
    SV80 = 0,
    SV256
};

/**
 * @class SVTrafficGen
 * @brief Create a banch of SV
 */
class SVTrafficGen
{
public:
    using Desc = SVPacketDesc;
    using TxSVUnit = TxUnit< SVPacketDesc >;
    using TxUnitArray = std::vector< SVTrafficGen::TxSVUnit >;
    SVTrafficGen(unsigned num, SV_TYPE type);

    inline size_t AmendPacketSV80(uint8_t *packet, const SVPacketDesc &desc) {
        SVSourceIED &ied = m_ieds[desc.idx];

        *(uint16_t *)(packet + m_appidOffset) = RTE_STATIC_BSWAP16((desc.idx + 1) & 0xFFFF);
        *(uint32_t *)(packet + m_asduOffs[0][SV_SVID_OFFSET] + 4) = *(uint32_t *)ied.sID;
        *(uint16_t *)(packet + m_asduOffs[0][SV_SMP_CNT_OFFSET]) = RTE_STATIC_BSWAP16(ied.smpCnt);

        ied.smpCnt = (ied.smpCnt + 1 < m_freq) ? (ied.smpCnt + 1) : 0;
        return m_skeletonSize;
    }

    inline size_t AmendPacketSV256(uint8_t *packet, const SVPacketDesc &desc) {
        SVSourceIED &ied = m_ieds[desc.idx];

        *(uint16_t *)(packet + m_appidOffset) = RTE_STATIC_BSWAP16((desc.idx + 1) & 0xFFFF);
        for (int i=0;i<MAX_SV_ASDU_NUM;++i) {
            *(uint32_t *)(packet + m_asduOffs[i][SV_SVID_OFFSET] + 4) = *(uint32_t *)ied.sID;
            *(uint16_t *)(packet + m_asduOffs[i][SV_SMP_CNT_OFFSET]) = RTE_STATIC_BSWAP16(ied.smpCnt);

            ied.smpCnt = (ied.smpCnt + 1 < m_freq) ? (ied.smpCnt + 1) : 0;
        }

        return m_skeletonSize;
    }

    size_t      GetSkeletonSize() const { return m_skeletonSize; }
    uint8_t*    GetSkeletonBuffer() { return m_skeleton; }

    SVTrafficGen::TxUnitArray& GetTxUnits() { return m_units; }   

private:
    void InitIED(SVSourceIED &ied, unsigned idx);
    void MakeSkeletonSV80();
    void MakeSkeletonSV256();
    std::vector< SVTrafficGen::TxSVUnit > CreateTxUnits(int pps);

private:
    std::vector< SVSourceIED >  m_ieds; // 1 SV <-> 1 IED
    SVTrafficGen::TxUnitArray   m_units; // TX moments with {blocks}

    uint16_t    m_appidOffset = 0; // One for the whole packet
    uint16_t    m_asduOffs[MAX_SV_ASDU_NUM][SV_ASDU_OFFSET_NUM] = { 0 };
    uint8_t     m_skeleton[MAX_SV_PACKET_SIZE] = { 0 };
    size_t      m_skeletonSize = 0;
    unsigned    m_freq = 1;
};

