#include "goose_traffic_gen.hpp"

#include "mms_value.h"
#include "goose_publisher.h"
#include "goose_receiver.h"

#include <iostream>
#include <inttypes.h>
#include <stdexcept>

#define GOOSE_MAX_MESSAGE_SIZE      1500

/**
 * @note 
 */
#define PLACEHOLDER                 "12345678"

const char *GOID_PATTERN     = "GOID" PLACEHOLDER;
const char *GOCB_REF_PATTERN = "IED" PLACEHOLDER "LDName/LLN0$GO$GOCB";
const char *DS_REF_PATTERN   = "IED" PLACEHOLDER "LDName/LLN0$DataSet";

namespace 
{
    void print_goose_offsets(uint16_t offsets[])
    {
        std::cout << "Goose offsets:\n"
                  << "\t AppID = " << offsets[GOOSE_APPID_OFFSET] << "\n"
                  << "\t GOID = " << offsets[GOOSE_GOID_OFFSET] << "\n"
                  << "\t GOCB_REF = " << offsets[GOOSE_GOCB_REF_OFFSET] << "\n"
                  << "\t DS_REF = " << offsets[GOOSE_DS_REF_OFFSET] << "\n"
                  << "\t SQ_NUM = " << offsets[GOOSE_SQ_NUM_OFFSET] << "\n"
                  << "\t ST_NUM = " << offsets[GOOSE_ST_NUM_OFFSET] << "\n"
                  << "\t D1 = " << offsets[GOOSE_D1_OFFSET] << "\n";
    }
}

GooseTrafficGen::GooseTrafficGen(unsigned MaxGooseNum, unsigned SndFreq, unsigned SignalsPerGoose)
    : m_ieds(MaxGooseNum)
{
    for (size_t i=0;i<m_ieds.size();++i) {
        InitIED(m_ieds[i], i + 1);
    }

    MakeSkeletonPacket(SignalsPerGoose);

    GenerateTxUnits(SndFreq);
}

void GooseTrafficGen::InitIED(GooseSourceIED &ied, unsigned idx)
{
	snprintf(ied.sID, sizeof(ied.sID), "%08" PRIu64 "", (uint64_t)idx);
}

void GooseTrafficGen::MakeSkeletonPacket(unsigned sigNum)
{
    CommParameters gooseCommParameters = {
        .vlanPriority = 4,
        .vlanId = 0,
        .appId = 0x1234,
        .dstAddress = { 0x01, 0x0C, 0xCD, 0x04, 0x00, 0x00 }
    };

    // Dataset items:
    LinkedList dataSetValues = LinkedList_create();
    for (size_t i=0;i<sigNum;++i) {
        LinkedList_add(dataSetValues, MmsValue_newBoolean(i % 2));
    }

    GoosePublisher publisher = GoosePublisher_create(&gooseCommParameters, "lo");
    if (publisher) {
        GoosePublisher_setGoID(publisher, const_cast< char* >(GOID_PATTERN));
        GoosePublisher_setGoCbRef(publisher, const_cast< char* >(GOCB_REF_PATTERN));
        GoosePublisher_setDataSetRef(publisher, const_cast< char* >(DS_REF_PATTERN));
        GoosePublisher_setConfRev(publisher, 1);
        GoosePublisher_setTimeAllowedToLive(publisher, 2000);
        GoosePublisher_setStNum(publisher, 0x7FFFFFFF);
        GoosePublisher_setSqNum(publisher, 0x7FFFFFFF);

        int retval = GoosePublisher_generateMessage(
                        publisher, &gooseCommParameters, dataSetValues,
                        m_skeleton, GOOSE_MAX_MESSAGE_SIZE, &m_skeletonSize
                    );
        if (retval == -1) {
            throw std::invalid_argument("Can't generate GOOSE message with libiec61850 "
                                        + std::to_string(retval));
        }

        // Source MAC
        const uint8_t SMAC[6] = { 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5 };
        std::memcpy(m_skeleton + 6, SMAC, 6);

		// Offsets
		GooseMessageOffsets offs;
		if (GooseReceiver_getMessageOffsets(m_skeleton, m_skeletonSize, &offs)) {
			m_offsets[GOOSE_APPID_OFFSET] = offs.appid;
			m_offsets[GOOSE_GOID_OFFSET] = offs.goid;
			m_offsets[GOOSE_GOCB_REF_OFFSET] = offs.gocb;
			m_offsets[GOOSE_DS_REF_OFFSET] = offs.dataset;
            m_offsets[GOOSE_TIMESTAMP_OFFSET] = offs.timestamp;
			m_offsets[GOOSE_SQ_NUM_OFFSET] = offs.sqNum;
			m_offsets[GOOSE_ST_NUM_OFFSET] = offs.stNum;
			m_offsets[GOOSE_D1_OFFSET] = offs.value;
		} else {
            throw std::invalid_argument("Can't find GOOSE's offsets");
        }

        /* print_goose_offsets(m_offsets); */
        GoosePublisher_destroy(publisher);
        LinkedList_destroy(dataSetValues);
    }
}

void GooseTrafficGen::GenerateTxUnits(unsigned pps)
{
    const unsigned BUNCH_SIZE = 32;
    m_units.reserve(pps);
    for (size_t i=0;i<pps;i++) {
        TxGooseUnit unit;
        unit.offsetUS = 1'000'000 / pps * i;

        const unsigned BLK_PER_UNIT = m_ieds.size() / BUNCH_SIZE;
        const unsigned LAST_BLK_SIZE = m_ieds.size() % BUNCH_SIZE;
        unit.blocks.resize(BLK_PER_UNIT + (LAST_BLK_SIZE > 0 ? 1 : 0));

        unsigned iedIndex = 0;
        for (size_t j=0;j<BLK_PER_UNIT;++j) {
            TxGooseUnit::BlockType &blk = unit.blocks[j];
            blk.packets.resize(BUNCH_SIZE);
            for (auto &desc : blk.packets) {
                desc.idx = iedIndex++;
            }
        }
        if (LAST_BLK_SIZE > 0) {
            TxGooseUnit::BlockType &blk = unit.blocks.back();
            blk.packets.resize(LAST_BLK_SIZE);
            for (auto &desc : blk.packets) {
                desc.idx = iedIndex++;
            }
        }

        m_units.push_back(std::move(unit));
    }
}

