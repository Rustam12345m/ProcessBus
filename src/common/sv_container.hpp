#pragma once

#include "mac_addr.hpp"

#include <format>
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
        return (appid == r.appid)
                && (std::memcmp(dmac, r.dmac, sizeof(dmac)) == 0)
                && (num == r.num)
                && (goid == r.goid);
    }

    // Hash functor
    std::size_t operator()(const SVStreamPassport& k) const {
        return appid;
    }

    // Equality functor
    bool operator()(SVStreamPassport &l, SVStreamPassport &r) const {
        return (l == r);
    }

    friend std::ostream& operator<<(std::ostream &out, const SVStreamPassport &obj) {
        out << obj.dmac << "\n"
            << std::format("\tAPPID = {:04X}\n", obj.appid)
            << "\nCRev = " << obj.crev << "\n"
            << "\nNum = " << obj.num << "\n"
            << "\tSVID = " << obj.svid << "\n";
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

