#include "bus_generator/sv_traffic_gen.hpp"
#include "bus_processor/process_bus_parser.hpp"

// SV generation by libiec61850
#include "mms_value.h"
#include "sv_publisher.h"

#include "goose_receiver.h"
#include "sv_subscriber.h"

#include <gtest/gtest.h>
#include <stdexcept>

#define MAX_PACKET_SIZE     1518

class SVMakerByLib
{
public:
    size_t MakePacket(uint8_t *buffer)
    {
        CommParameters ethParams = {
            .vlanPriority = 4,
            .vlanId = 101,
            .appId = m_appid,
            .dstAddress = { 0x01, 0x0C, 0xCD, 0x01, 0x00, 0x01 }
        };

        size_t packetSize = 0;
        SVPublisher publisher = SVPublisher_create(&ethParams, "lo");
        if (publisher) {
            // SV80
            SVPublisher_ASDU asdu = SVPublisher_addASDU(publisher, m_svid.c_str(), NULL, m_crev);
            if (asdu == nullptr) {
                throw std::runtime_error("Can't create SVPublisher_ASDU!");
            }

            int vIndex[8] = { 0 }, qIndex[8] = { 0 };
            for (int i=0;i<8;++i) {
                vIndex[i] = SVPublisher_ASDU_addINT32(asdu);
                qIndex[i] = SVPublisher_ASDU_addQuality(asdu);
            }
            SVPublisher_setupComplete(publisher);

            SVPublisher_ASDU_setINT32(asdu, vIndex[0], 1);
            SVPublisher_ASDU_setINT32(asdu, vIndex[1], 2);
            SVPublisher_ASDU_setINT32(asdu, vIndex[2], 3);
            SVPublisher_ASDU_setINT32(asdu, vIndex[3], 4);
            SVPublisher_ASDU_increaseSmpCnt(asdu);

            int size = 0;
            uint8_t *packet = nullptr;
            SVPublisher_getBuffer(publisher, &packet, &size);
            if ((packet != nullptr) && (size > 0)) {
                memcpy(buffer, packet, size);
                packetSize = size;
            }

            SVPublisher_destroy(publisher);
        } else {
            throw std::runtime_error("Can't create SVPublisher from libiec61850!");
        }
        return packetSize;
    }

    SVMakerByLib& SetAppID(uint16_t appid) {
        m_appid = appid;
        return *this;
    }
    SVMakerByLib& SetSVID(const std::string &id) {
        m_svid = id;
        return *this;
    }
    SVMakerByLib& SetCRev(uint32_t crev) {
        m_crev = crev;
        return *this;
    }

    uint16_t         GetAppID() const {
        return m_appid;
    }
    std::string      GetSVID() const {
        return m_svid;
    }
    uint32_t         GetCRev() const {
        return m_crev;
    }

private:
    uint16_t    m_appid = 0x0000;
    std::string m_svid = "DefaultSVID";
    uint32_t    m_crev = 1;
};

TEST(BusGenerator, SVTrafficBasicUsage)
{
}

TEST(SVFastParser, BasicUsage)
{
    uint8_t packet[MAX_PACKET_SIZE] = { 0 };
    size_t size = 256;
    SVStreamPassport passport;
    SVStreamState state;

    int retval = ProcessBusParser::parse_sv_packet(packet, size, passport, state);
    ASSERT_NE(retval, 0);

    SVMakerByLib sv80;
    size = sv80.SetAppID(777)
                .SetSVID("Test_SV_ID")
                .MakePacket(packet);
    retval = ProcessBusParser::parse_sv_packet(packet, size, passport, state);
    ASSERT_EQ(retval, 0) << "Can't parse packet: Size = " << size;
    //std::cout << passport;

    ASSERT_EQ(passport.appid, sv80.GetAppID()) << passport;
    ASSERT_EQ(passport.svid, sv80.GetSVID()) << passport;
}

TEST(SVStreamContainer, BasicUsage)
{
    SVContainer svStreamMap;

    SVStreamPassport passport;
    ASSERT_EQ(svStreamMap.find(passport), svStreamMap.end());
    
    // Test SV packet
    uint8_t packet[MAX_PACKET_SIZE] = { 0 };
    SVMakerByLib svByLib;
    SVStreamState state;
    size_t packetSize = svByLib.MakePacket(packet);
    int retval = ProcessBusParser::parse_sv_packet(packet, packetSize, passport, state);
    ASSERT_EQ(retval, 0) << passport;

    // SV 1
    auto stream1 = std::make_shared< SVStreamSource >();
    stream1->SetMAC(MAC("01:0C:CD:01:12:34"))
        .SetAppID(0x1111)
        .SetSVID("TestSVID1")
        .SetCRev(54321);
    svStreamMap[stream1->GetPassport()] = stream1;

    // SV 2
    auto stream2 = std::make_shared< SVStreamSource >();
    stream2->SetMAC(MAC("01:0C:CD:01:12:34"))
        .SetAppID(0x2222)
        .SetSVID("TestSVID2")
        .SetCRev(54321);
    svStreamMap[stream2->GetPassport()] = stream2;

    auto it = svStreamMap.find(passport);
    ASSERT_EQ(it, svStreamMap.end());

    // Check SV1
    auto itStream1 = svStreamMap.find(stream1->GetPassport());
    ASSERT_NE(itStream1, svStreamMap.end());
    ASSERT_NE(itStream1->second, nullptr);
    ASSERT_EQ(itStream1->second->GetPassport(), stream1->GetPassport());
    ASSERT_NE(itStream1->second->GetPassport(), stream2->GetPassport());
    ASSERT_NE(passport, stream1->GetPassport());

    // Check SV2
    auto itStream2 = svStreamMap.find(stream2->GetPassport());
    ASSERT_NE(itStream2, svStreamMap.end());
    ASSERT_NE(itStream2->second, nullptr);
    ASSERT_EQ(itStream2->second->GetPassport(), stream2->GetPassport());
    ASSERT_NE(itStream2->second->GetPassport(), stream1->GetPassport());
    ASSERT_NE(passport, stream2->GetPassport());
}

