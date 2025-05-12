#pragma once

#include "mac_addr.hpp"

#include <unordered_map>
#include <memory>

/**
 * @class GoosePassport
 * @brief Unique description of GOOSE
 */
struct GoosePassport
{
    MAC                 dmac;
    uint16_t            appid = 0;
    uint16_t            num = 0;
    uint32_t            crev = 0;

    std::string_view    goid;
    std::string_view    dataset;
    std::string_view    gocbref;

    bool operator==(const GoosePassport &r) const {
        return (dmac == r.dmac)
                && (appid == r.appid)
                && (goid == r.goid)
                && (dataset == r.dataset)
                && (gocbref == r.gocbref)
                && (crev == r.crev)
                && (num == r.num);
    }

    friend std::ostream& operator<<(std::ostream &out, const GoosePassport &obj) {
        out << "\tDMAC = " << obj.dmac << "\n"
            << std::format("\tAPPID = {:04X}\n", obj.appid)
            << "\tGOID = " << obj.goid << "\n"
            << "\tDATASET = " << obj.dataset << "\n"
            << "\tGOCB = " << obj.gocbref << "\n"
            << "\tNumEntries = " << obj.num << "\n"
            << "\tCRev = " << obj.crev;
        return out;
    }
};

struct GoosePassportHash
{
    size_t operator()(const GoosePassport& key) const {
        return key.appid;
    }
};

struct GoosePassportEqual
{
    bool operator()(const GoosePassport& kv1, const GoosePassport& kv2) const {
        return (kv1 == kv2);
    }
};

struct GooseState
{
    // State
    uint64_t            timestamp = 0;
    uint32_t            stNum = 0;
    uint32_t            sqNum = 0;

    friend std::ostream& operator<<(std::ostream &out, const GooseState &obj) {
        out << "\tSqNum = " << obj.sqNum << "\n"
            << "\tStNum = " << obj.stNum;
        return out;
    }
};

/**
 * @struct GooseSource
 * @brief
 */
class GooseSource
{
public:
    using ptr = std::shared_ptr< GooseSource >;
    GooseSource() {}

    GooseSource&    SetMAC(const MAC mac) {
        m_dmac = mac;
        return *this;
    }
    GooseSource&    SetAppID(uint16_t appid) {
        m_appid = appid;
        return *this;
    }
    GooseSource&    SetDataSetRef(const std::string &dataset) {
        m_dataSetRef = dataset;
        return *this;
    }
    GooseSource&    SetGOID(const std::string &goid) {
        m_goid = goid;
        return *this;
    }
    GooseSource&    SetGOCBRef(const std::string &gocb) {
        m_gocbRef = gocb;
        return *this;
    }
    GooseSource&    SetCRev(uint32_t crev) {
        m_crev = crev;
        return *this;
    }
    GooseSource&    SetNumEntries(uint32_t num) {
        m_numEntries = num;
        return *this;
    }

    uint16_t        GetAppID() const {
        return m_appid;
    }
    std::string     GetGOID() const {
        return m_goid;
    }
    std::string     GetGOCBRef() const {
        return m_gocbRef;
    }
    std::string     GetDataSetRef() const {
        return m_dataSetRef;
    }
    uint32_t        GetCRev() const {
        return m_crev;
    }

    GoosePassport   GetPassport() const {
        GoosePassport pass;
        pass.dmac = m_dmac;
        pass.appid = m_appid;
        pass.crev = m_crev;
        pass.num = m_numEntries;
        pass.goid = m_goid;
        pass.dataset = m_dataSetRef;
        pass.gocbref = m_gocbRef;
        return pass;
    }

    void            ProcessState(const GoosePassport &pass,
                                 const GooseState &state) {
        if (m_stNum != state.stNum) {
            if (state.stNum != m_stNum + 1) {
                ++m_errSeqCnt;
            }
        }
        m_stNum = state.stNum;
        m_sqNum = state.sqNum;
    }

    friend std::ostream& operator<<(std::ostream &out, const GooseSource &obj) {
        out << obj.GetPassport()
            << "\nState:\n"
            << "\tSqNum = " << obj.m_sqNum << "\n"
            << "\tStNum = " << obj.m_stNum << "\n"
            << "\tErrSeqCnt = " << obj.m_errSeqCnt << "\n";
        return out;
    }

private:
    // Settings
    MAC         m_dmac;
    uint16_t    m_appid = 0;
    std::string m_goid;
    std::string m_dataSetRef;
    std::string m_gocbRef;
    uint32_t    m_crev = 0;
    uint32_t    m_numEntries = 0;

    // State
    uint32_t    m_stNum = 0, m_sqNum = 0;
    uint32_t    m_errSeqCnt = 0;
};

using GooseContainer = std::unordered_map<
                                GoosePassport,      /* Key type */
                                GooseSource::ptr,   /* Value type */
                                GoosePassportHash,  /* Custom hash operator */
                                GoosePassportEqual  /* Custom equality operator */
                            >;

