#include "bus_generator/goose_traffic_gen.hpp"
#include "bus_processor/process_bus_parser.hpp"

// GOOSE generation by libiec61850
#include "mms_value.h"
#include "goose_publisher.h"

#include "goose_receiver.h"
#include "goose_subscriber.h"

#include <gtest/gtest.h>
#include <stdexcept>

#define MAX_PACKET_SIZE     1518

class GooseParserByLib
{
public:
    GooseParserByLib()
    {
        char port[8] = "";
        m_subscriber = GooseSubscriber_create(port, NULL);
        if (m_subscriber == NULL) {
            throw std::runtime_error("Can't create GooseSubscriber from libiec61850!");
        }
        GooseSubscriber_setObserver(m_subscriber);
        GooseSubscriber_setListener(m_subscriber, GooseParserByLib::callback_handler, this);

        m_receiver = GooseReceiver_create();
        if (m_receiver == NULL) {
            throw std::runtime_error("Can't create GooseReceiver from libiec61850!");
        }
        GooseReceiver_addSubscriber(m_receiver, m_subscriber);
    }

    int ParseGoose(uint8_t *buffer, size_t size)
    {
        m_isFound = false;
        GooseReceiver_handleMessage(m_receiver, buffer, size);
        return m_isFound ? 0 : -1;
    }

    static void callback_handler(GooseSubscriber subscriber, void* parameter)
    {
        GooseParserByLib *obj = reinterpret_cast< GooseParserByLib* >(parameter);
        obj->m_isFound = true;

        /* print_goose(obj->m_subscriber); */
    }

    static void print_goose(GooseSubscriber subscriber)
    {
        printf("GOOSE message:\n");
        printf("\tvlanTag: %s\n", GooseSubscriber_isVlanSet(subscriber) ? "found" : "NOT found");
        if (GooseSubscriber_isVlanSet(subscriber)) {
            printf("\t\tvlanId: %u\n", GooseSubscriber_getVlanId(subscriber));
            printf("\t\tvlanPrio: %u\n", GooseSubscriber_getVlanPrio(subscriber));
        }

        printf("\tappId: %d\n", GooseSubscriber_getAppId(subscriber));
        uint8_t macBuf[6];
        GooseSubscriber_getSrcMac(subscriber,macBuf);
        printf("\tsrcMac: %02X:%02X:%02X:%02X:%02X:%02X\n",
                macBuf[0],macBuf[1],macBuf[2],macBuf[3],macBuf[4],macBuf[5]);
        GooseSubscriber_getDstMac(subscriber,macBuf);
        printf("\tdstMac: %02X:%02X:%02X:%02X:%02X:%02X\n",
                macBuf[0],macBuf[1],macBuf[2],macBuf[3],macBuf[4],macBuf[5]);
        printf("\tgoId: %s\n", GooseSubscriber_getGoId(subscriber));
        printf("\tgoCbRef: %s\n", GooseSubscriber_getGoCbRef(subscriber));
        printf("\tdataSet: %s\n", GooseSubscriber_getDataSet(subscriber));
        printf("\tconfRev: %u\n", GooseSubscriber_getConfRev(subscriber));
        printf("\tndsCom: %s\n", GooseSubscriber_needsCommission(subscriber) ? "true" : "false");
        printf("\tsimul: %s\n", GooseSubscriber_isTest(subscriber) ? "true" : "false");
        printf("\tstNum: %u sqNum: %u\n", GooseSubscriber_getStNum(subscriber),
                GooseSubscriber_getSqNum(subscriber));
        printf("\ttimeToLive: %u\n", GooseSubscriber_getTimeAllowedToLive(subscriber));

        uint64_t timestamp = GooseSubscriber_getTimestamp(subscriber);
        printf("\ttimestamp: %u.%u\n", (uint32_t) (timestamp / 1000), (uint32_t) (timestamp % 1000));
        printf("\tmessage is %s\n", GooseSubscriber_isValid(subscriber) ? "valid" : "INVALID");

        char buffer[1024];
        MmsValue* values = GooseSubscriber_getDataSetValues(subscriber);
        MmsValue_printToBuffer(values, buffer, 1024);
        printf("\tAllData: %s\n", buffer);
    }

private:
    GooseReceiver   m_receiver;
    GooseSubscriber m_subscriber;
    bool            m_isFound = false;
};

class GooseMakerByLib
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

        LinkedList dataSetValues = LinkedList_create();
        for (unsigned i=0;i<m_numEntries;++i) {
            LinkedList_add(dataSetValues, MmsValue_newBoolean(i % 2 == 0));
        }

        size_t packetSize = 0;
        GoosePublisher publisher = GoosePublisher_create(&ethParams, "lo");
        if (publisher) {
            GoosePublisher_setGoCbRef(publisher, GetGOCBRef().c_str());
            GoosePublisher_setDataSetRef(publisher, GetDataSetRef().c_str());
            GoosePublisher_setGoID(publisher, m_goid.c_str());
            GoosePublisher_setConfRev(publisher, m_crev);
            GoosePublisher_setTimeAllowedToLive(publisher, 500);

            int retval = GoosePublisher_generateMessage(publisher, &ethParams, 
                                                        dataSetValues,
                                                        buffer, MAX_PACKET_SIZE, 
                                                        &packetSize);
            if (retval < 0 || packetSize == 0) {
                throw std::runtime_error("Can't generate GOOSE with libiec61850"
                                         + std::to_string(retval));
                return 0;
            }

            GoosePublisher_destroy(publisher);
            LinkedList_destroy(dataSetValues);
        } else {
            throw std::runtime_error("Can't create GoosePublisher from libiec61850!");
        }
        return packetSize;
    }

    GooseMakerByLib& SetAppID(uint16_t appid) {
        m_appid = appid;
        return *this;
    }
    GooseMakerByLib& SetIED(const std::string &ied) {
        m_iedName = ied;
        return *this;
    }
    GooseMakerByLib& SetLD(const std::string &ld) {
        m_ldName = ld;
        return *this;
    }
    GooseMakerByLib& SetDataSet(const std::string &dataset) {
        m_dataSet = dataset;
        return *this;
    }
    GooseMakerByLib& SetGOID(const std::string &goid) {
        m_goid = goid;
        return *this;
    }
    GooseMakerByLib& SetGOCB(const std::string &gocb) {
        m_gocb = gocb;
        return *this;
    }
    GooseMakerByLib& SetCRev(uint32_t crev) {
        m_crev = crev;
        return *this;
    }

    uint16_t         GetAppID() const {
        return m_appid;
    }
    std::string      GetGOID() const {
        return m_goid;
    }
    std::string      GetGOCBRef() const {
        return m_iedName + m_ldName + "/LLN0$GO$" + m_gocb;
    }
    std::string      GetDataSetRef() const {
        return m_iedName + m_ldName + "/LLN0$" + m_dataSet;
    }
    uint32_t         GetCRev() const {
        return m_crev;
    }
    uint32_t         GetNumEntries() const {
        return m_numEntries;
    }

private:
    uint16_t    m_appid = 0x0000;
    std::string m_iedName = "DefaultIED";
    std::string m_ldName = "DefaultLD";
    std::string m_gocb = "DefaultGOCB";
    std::string m_dataSet = "DefaultDataSet";
    std::string m_goid = "DefaultGOID";
    uint32_t    m_crev = 1;
    uint32_t    m_numEntries = 4;
};


TEST(BusGenerator, BasicUsage)
{
    // Generate a bunch of Goose messages for fast modify
    const unsigned GooseNum = 10, SndFreq = 1, SignalNum = 16;
    GooseTrafficGen gen(GooseNum, SndFreq, SignalNum);

    std::vector< uint8_t > buffer(MAX_PACKET_SIZE);
    memcpy(buffer.data(), gen.GetSkeletonBuffer(), gen.GetSkeletonSize());

    auto units = gen.GetTxUnits();
    for (const auto &unit : units) {
        for (size_t i=0;i<unit.blocks.size();++i) {
            auto &blk = unit.blocks[i];

            for (size_t j=0;j<blk.packets.size();++j) {
                gen.AmendPacket(buffer.data(), blk.packets[j]);

                // Check the packet validity
            }
            // Sending this block
        }
    }
}

TEST(BusGenerator, CheckPacketsByLibiec61850)
{
    GooseParserByLib gooseLibPaser;

    // Generate a bunch of Goose messages for fast modify
    const unsigned GooseNum = 10, SndFreq = 10, SignalNum = 16;
    /* const unsigned GooseNum = 1000, SndFreq = 1000, SignalNum = 16; */
    GooseTrafficGen gen(GooseNum, SndFreq, SignalNum);

    std::vector< uint8_t > buffer(MAX_PACKET_SIZE);
    memcpy(buffer.data(), gen.GetSkeletonBuffer(), gen.GetSkeletonSize());

    auto &units = gen.GetTxUnits();
    for (const auto &unit : units) {
        for (size_t i=0;i<unit.blocks.size();++i) {
            auto &blk = unit.blocks[i];

            for (size_t j=0;j<blk.packets.size();++j) {
                gen.AmendPacket(buffer.data(), blk.packets[j]);

                // Check the packet validity by libiec61850
                int retval = gooseLibPaser.ParseGoose(buffer.data(), gen.GetSkeletonSize());
                ASSERT_EQ(retval, 0) << "Block = " << i << ", packet = " << j;
            }
        }
    }
}

TEST(GooseFastParser, BasicUsage)
{
    uint8_t packet[MAX_PACKET_SIZE] = { 0 };
    size_t size = 256;
    GoosePassport passport;
    GooseState state;

    int retval = parse_goose_packet(packet, size, passport, state);
    ASSERT_NE(retval, 0);

    GooseMakerByLib goose;
    size = goose.SetAppID(777)
                .SetIED("IEDName")
                .SetLD("LDName")
                .SetGOCB("GOCBName")
                .SetDataSet("DataSetName")
                .SetCRev(123)
                .MakePacket(packet);
    retval = parse_goose_packet(packet, size, passport, state);
    ASSERT_EQ(retval, 0) << "Can't parse packet: Size = " << size;

    /* std::cout << passport; */
    ASSERT_EQ(passport.appid, goose.GetAppID()) << passport;
    ASSERT_EQ(passport.goid, goose.GetGOID()) << passport;
    ASSERT_EQ(passport.gocbref, goose.GetGOCBRef()) << passport;
    ASSERT_EQ(passport.dataset, goose.GetDataSetRef()) << passport;
    ASSERT_EQ(passport.crev, goose.GetCRev()) << passport;
    ASSERT_EQ(passport.num, goose.GetNumEntries()) << passport;
}

TEST(GooseFastParser, RealPacket)
{
    uint8_t packet[] = {
        0x01, 0x0C, 0xCD, 0x04, 0x00, 0x00, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 
        0x88, 0xB8, 0x00, 0x01, 0x00, 0xB1, 0x00, 0x00, 0x00, 0x00, 0x61, 0x81, 
        0xA6, 0x80, 0x1E, 0x49, 0x45, 0x44, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 
        0x30, 0x31, 0x4C, 0x44, 0x4E, 0x61, 0x6D, 0x65, 0x2F, 0x4C, 0x4C, 0x4E, 
        0x30, 0x24, 0x47, 0x4F, 0x24, 0x47, 0x4F, 0x43, 0x42, 0x81, 0x02, 0x07, 
        0xD0, 0x82, 0x1E, 0x49, 0x45, 0x44, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 
        0x30, 0x31, 0x4C, 0x44, 0x4E, 0x61, 0x6D, 0x65, 0x2F, 0x4C, 0x4C, 0x4E, 
        0x30, 0x24, 0x44, 0x61, 0x74, 0x61, 0x53, 0x65, 0x74, 0x83, 0x0C, 0x47, 
        0x4F, 0x49, 0x44, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0x84, 
        0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x85, 0x04, 0x00, 
        0x00, 0x00, 0x04, 0x86, 0x04, 0x00, 0x00, 0x00, 0x00, 0x87, 0x01, 0x00, 
        0x88, 0x01, 0x01, 0x89, 0x01, 0x00, 0x8A, 0x01, 0x10, 0xAB, 0x30, 0x83, 
        0x01, 0x01, 0x83, 0x01, 0x01, 0x83, 0x01, 0x00, 0x83, 0x01, 0x01, 0x83, 
        0x01, 0x00, 0x83, 0x01, 0x01, 0x83, 0x01, 0x00, 0x83, 0x01, 0x01, 0x83, 
        0x01, 0x00, 0x83, 0x01, 0x01, 0x83, 0x01, 0x00, 0x83, 0x01, 0x01, 0x83, 
        0x01, 0x00, 0x83, 0x01, 0x01, 0x83, 0x01, 0x00, 0x83, 0x01, 0x01
    };

    GoosePassport passport;
    GooseState state;

    int retval = parse_goose_packet(packet, sizeof(packet), passport, state);
    ASSERT_EQ(retval, 0);

    GooseParserByLib gooseLibPaser;
    retval = gooseLibPaser.ParseGoose(packet, sizeof(packet)); 
    ASSERT_EQ(retval, 0);
}

TEST(GooseContainer, BasicUsage)
{
    GooseContainer gooseMap;

    GoosePassport passport;
    ASSERT_EQ(gooseMap.find(passport), gooseMap.end());

    // Test GOOSE packet
    uint8_t packet[MAX_PACKET_SIZE] = { 0 };
    GooseMakerByLib goose;
    GooseState state;
    size_t packetSize = goose.MakePacket(packet);
    int retval = parse_goose_packet(packet, packetSize, passport, state);
    ASSERT_EQ(retval, 0) << passport;

    // GOOSE 1
    auto g1 = std::make_shared< GooseSource >();
    g1->SetMAC(MAC("01:0C:CD:01:12:34"))
        .SetAppID(0x1111)
        .SetDataSetRef("TestDataSetRef1")
        .SetGOCBRef("TestGOCBRef1")
        .SetGOID("TestGOID1")
        .SetCRev(54321)
        .SetNumEntries(16);
    gooseMap[g1->GetPassport()] = g1;

    // GOOSE 2
    auto g2 = std::make_shared< GooseSource >();
    g2->SetMAC(MAC("01:0C:CD:01:12:34"))
        .SetAppID(0x2222)
        .SetDataSetRef("TestDataSetRef2")
        .SetGOCBRef("TestGOCBRef2")
        .SetGOID("TestGOID2")
        .SetCRev(54321)
        .SetNumEntries(16);
    gooseMap[g2->GetPassport()] = g2;

    auto it = gooseMap.find(passport);
    ASSERT_EQ(it, gooseMap.end());

    // Check GOOSE1
    auto itG1 = gooseMap.find(g1->GetPassport());
    ASSERT_NE(itG1, gooseMap.end());
    ASSERT_NE(itG1->second, nullptr);
    ASSERT_EQ(itG1->second->GetPassport(), g1->GetPassport());
    ASSERT_NE(itG1->second->GetPassport(), g2->GetPassport());
    ASSERT_NE(passport, g1->GetPassport());

    // Check GOOSE2
    auto itG2 = gooseMap.find(g2->GetPassport());
    ASSERT_NE(itG2, gooseMap.end());
    ASSERT_NE(itG2->second, nullptr);
    ASSERT_EQ(itG2->second->GetPassport(), g2->GetPassport());
    ASSERT_NE(itG2->second->GetPassport(), g1->GetPassport());
    ASSERT_NE(passport, g2->GetPassport());
}

