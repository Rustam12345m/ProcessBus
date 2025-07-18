#pragma once

#include "mac_addr.hpp"

#include <format>
#include <iostream>
#include <ostream>

/**
 * @class SVStreamPassport
 * @brief 
 */
struct SVStreamPassport
{
    MAC                 dmac;
    uint16_t            appid = 0;
    uint16_t            num = 0;
    uint32_t            crev = 0;
    std::string_view    svid;

    bool operator==(const SVStreamPassport &r) const {
        return (dmac == r.dmac)
                && (appid == r.appid)
                && (num == r.num)
                && (crev == r.crev)
                && (svid == r.svid);
    }

    // Hash functor
    std::size_t operator()(const SVStreamPassport& k) const {
        return appid;
    }

    // Equality functor
    bool operator()(const SVStreamPassport &l, const SVStreamPassport &r) const {
        return (l == r);
    }

    friend std::ostream& operator<<(std::ostream &out, const SVStreamPassport &obj) {
        out << "SVStreamPassport:\n"
            << "\tDMAC =  " << obj.dmac << "\n"
            << std::format(
               "\tAPPID = {:04X}\n", obj.appid)
            << "\tCRev =  " << obj.crev << "\n"
            << "\tNum =   " << obj.num << "\n"
            << "\tSVID =  " << obj.svid << "\n";
        return out;
    }
};

/**
 * @brief The state values from an SV-packet
 */
struct SVStreamState
{
    uint16_t smpCnt = 0;
};

class SVStreamSource
{
public:
    using ptr = std::shared_ptr< SVStreamSource >;

    SVStreamSource&    SetMAC(const MAC mac) {
        m_dmac = mac;
        return *this;
    }
    SVStreamSource&    SetAppID(uint16_t appid) {
        m_appid = appid;
        return *this;
    }
    SVStreamSource&    SetSVID(const std::string &svid) {
        m_svid = svid;
        return *this;
    }
    SVStreamSource&    SetCRev(uint32_t crev) {
        m_crev = crev;
        return *this;
    }
    SVStreamSource&    SetNumASDU(uint32_t num) {
        m_numASDU = num;
        return *this;
    }

    MAC             GetDMAC() const { 
        return m_dmac;
    }
    uint16_t        GetAppID() const {
        return m_appid;
    }
    std::string     GetSVID() const {
        return m_svid;
    }
    uint32_t        GetCRev() const {
        return m_crev;
    }
    uint32_t        GetSmpCnt() const {
        return m_smpCnt;
    }
    uint32_t        GetErrSeqNum() const {
        return m_errSmpCnt;
    }

    SVStreamPassport GetPassport() const {
        SVStreamPassport pass;
        pass.dmac = m_dmac;
        pass.appid = m_appid;
        pass.crev = m_crev;
        pass.svid = m_svid;
        pass.num = m_numASDU;
        return pass;
    }

    inline void ProcessState(const SVStreamPassport &pass,
                             const SVStreamState &state) {
        if ((state.smpCnt != (m_smpCnt + 1)) && (state.smpCnt != 0)) {
            ++m_errSmpCnt;
            /*
            std::cout << "Wrong smpCnt: " << m_smpCnt
                      << " in packet " << state.smpCnt
                      << "\n";
            */
        }
        m_smpCnt = state.smpCnt;
    }

    friend std::ostream& operator<<(std::ostream &out, const SVStreamSource &obj) {
        out << obj.GetPassport()
            << "\nState:\n"
            << "\tSmpCnt    = " << obj.m_smpCnt << "\n"
            << "\tErrSeqCnt = " << obj.m_errSmpCnt << "\n";
        return out;
    }

private:
    // Settings
    MAC         m_dmac;
    uint16_t    m_appid = 0;
    std::string m_svid;
    uint32_t    m_crev = 0;
    uint32_t    m_numASDU = 0;

    // State
    uint32_t    m_smpCnt = 0;
    uint32_t    m_errSmpCnt = 0;
};

using SVContainer = std::unordered_map<
                        SVStreamPassport,     /* Key type */
                        SVStreamSource::ptr,  /* Value type */
                        SVStreamPassport,     /* Custom hash operator */
                        SVStreamPassport      /* Custom equality operator */
                    >;

