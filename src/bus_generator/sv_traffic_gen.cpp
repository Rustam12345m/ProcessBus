#include "sv_traffic_gen.hpp"

#include "mms_value.h"
#include "sv_publisher.h"

#include <inttypes.h>
#include <stdexcept>

/**
 * @note 
 */
#define PLACEHOLDER             "1234"
const char *SVID_PATTERN = "SVID" PLACEHOLDER;

SVTrafficGen::SVTrafficGen(unsigned num, SV_TYPE type) : m_ieds(num)
{
    for (size_t i=0;i<m_ieds.size();++i) {
        InitIED(m_ieds[i], i + 1);
    }

    switch (type) {
    case SV80: {
        m_freq = 4000;
        MakeSkeletonSV80();
        m_units = CreateTxUnits(m_freq);
        break;
    }
    case SV256: {
        m_freq = 12800;
        MakeSkeletonSV256();
        m_units = CreateTxUnits(m_freq / MAX_SV_ASDU_NUM);
        break;
    }
    default: {
        throw std::invalid_argument("Unknown SV type");
    }
    }
}

void SVTrafficGen::InitIED(SVSourceIED &ied, unsigned idx)
{
    snprintf(ied.sID, sizeof(ied.sID), "%04u", idx);
}

void SVTrafficGen::MakeSkeletonSV80()
{
    SVPublisher sv80 = SVPublisher_create(NULL, "lo");
    if (sv80) {
        SVPublisher_ASDU asdu = SVPublisher_addASDU(sv80, SVID_PATTERN, NULL, 1);

        int vIndex[8] = { 0 }, qIndex[8] = { 0 };
        for (int i=0;i<8;++i) {
            vIndex[i] = SVPublisher_ASDU_addINT32(asdu);
            qIndex[i] = SVPublisher_ASDU_addQuality(asdu);
        }
        SVPublisher_setupComplete(sv80);

        m_appidOffset = SVPublisher_getAPPID_Offset(sv80);
        m_asduOffs[0][SV_SVID_OFFSET] = SVPublisher_ASDU_getSVID_Offset(sv80, asdu);
        m_asduOffs[0][SV_SMP_CNT_OFFSET] = SVPublisher_ASDU_getSmpCntOffset(sv80, asdu);
        m_asduOffs[0][SV_DATA_OFFSET] = SVPublisher_ASDU_getDataOffset(sv80, asdu);

        int packetSize = 0;
        uint8_t *intBuffer = nullptr;
        SVPublisher_getBuffer(sv80, &intBuffer, &packetSize);

        memcpy(m_skeleton, intBuffer, packetSize);
        m_skeletonSize = packetSize;

        SVPublisher_destroy(sv80);
    } else {
        throw std::invalid_argument("Failed to create SV publisher\n");
    }
}

void SVTrafficGen::MakeSkeletonSV256()
{
    SVPublisher sv256 = SVPublisher_create(NULL, "lo");
    if (sv256) {
        SVPublisher_ASDU asdu[MAX_SV_ASDU_NUM]; // 9.2LE 256 points
        for (int i=0;i<MAX_SV_ASDU_NUM;++i) {
            asdu[i] = SVPublisher_addASDU(sv256, SVID_PATTERN, NULL, 1);
            for (int j=0;j<8;++j) {
                SVPublisher_ASDU_addINT32(asdu[i]);
                SVPublisher_ASDU_addQuality(asdu[i]);
            }
        }
        SVPublisher_setupComplete(sv256);

        m_appidOffset = SVPublisher_getAPPID_Offset(sv256);
        for (int i=0;i<MAX_SV_ASDU_NUM;i++) {
            m_asduOffs[i][SV_SVID_OFFSET] = SVPublisher_ASDU_getSVID_Offset(sv256, asdu[i]);
            m_asduOffs[i][SV_SMP_CNT_OFFSET] = SVPublisher_ASDU_getSmpCntOffset(sv256, asdu[i]);
            m_asduOffs[i][SV_DATA_OFFSET] = SVPublisher_ASDU_getDataOffset(sv256, asdu[i]);
        }

        int packetSize = 0;
        uint8_t *intBuffer = nullptr;
        SVPublisher_getBuffer(sv256, &intBuffer, &packetSize);

        memcpy(m_skeleton, intBuffer, packetSize);
        m_skeletonSize = packetSize;

        SVPublisher_destroy(sv256);
    } else {
        throw std::invalid_argument("Failed to create SV publisher\n");
    }
}

SVTrafficGen::TxUnitArray SVTrafficGen::CreateTxUnits(int pps)
{
    std::vector< SVTrafficGen::TxSVUnit > units;

    const unsigned BUNCH_SIZE = 32;
    units.reserve(pps);
    for (size_t i=0;i<pps;i++) {
        TxSVUnit unit;
        unit.offsetUS = 1'000'000 / pps * i;

        const unsigned BLK_PER_UNIT = m_ieds.size() / BUNCH_SIZE;
        const unsigned LAST_BLK_SIZE = m_ieds.size() % BUNCH_SIZE;
        unit.blocks.resize(BLK_PER_UNIT + (LAST_BLK_SIZE > 0 ? 1 : 0));

        unsigned iedIndex = 0;
        for (size_t j=0;j<BLK_PER_UNIT;++j) {
            TxSVUnit::BlockType &blk = unit.blocks[j];
            blk.packets.resize(BUNCH_SIZE);
            for (auto &desc : blk.packets) {
                desc.idx = iedIndex++;
            }
        }
        if (LAST_BLK_SIZE > 0) {
            TxSVUnit::BlockType &blk = unit.blocks.back();
            blk.packets.resize(LAST_BLK_SIZE);
            for (auto &desc : blk.packets) {
                desc.idx = iedIndex++;
            }
        }

        units.push_back(std::move(unit));
    }
    return std::move(units);
}

