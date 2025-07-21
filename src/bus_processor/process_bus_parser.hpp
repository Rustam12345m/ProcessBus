#pragma once

#include "common/goose_container.hpp"
#include "common/sv_container.hpp"
#include <rte_byteorder.h>

enum BUS_PROTO
{
    NON_BUS_PROTO = 0,
    BUS_PROTO_SV,
    BUS_PROTO_GOOSE,
    /* BUS_PROTO_PTP */
};

class ProcessBusParser
{
public:
    static inline
    unsigned get_appid(const uint8_t* buffer)
    {
        return RTE_STATIC_BSWAP16(*(uint16_t *)(buffer + 18));
    }

    /**
     * @function get_proto_type
     * @brief Is used to get APPID and dispatch GOOSE/SV mbuf to a particular CPU
     */
    static inline
    BUS_PROTO get_proto_type(const uint8_t* buffer, unsigned *appid)
    {
        if (buffer[12] == 0x81 && buffer[13] == 0x00) {
            // VLAN
            if (buffer[16] == 0x88 && buffer[17] == 0xBA) {
                // SV
                *appid = get_appid(buffer + 18);
                return BUS_PROTO_SV;
            }
            if (buffer[16] == 0x88 && buffer[17] == 0xB8) {
                // GOOSE
                *appid = get_appid(buffer + 18);
                return BUS_PROTO_GOOSE;
            }
        }
        if (buffer[12] == 0x88 && buffer[13] == 0xBA) {
            // SV without VLAN
            *appid = get_appid(buffer + 14);
            return BUS_PROTO_SV;
        }
        if (buffer[12] == 0x88 && buffer[13] == 0xB8) {
            // GOOSE without VLAN
            *appid = get_appid(buffer + 14);
            return BUS_PROTO_GOOSE;
        }
        return NON_BUS_PROTO;
    }

    /**
     * @function parse_goose_packet
     */
    static int
    parse_goose_packet(const uint8_t *buffer, int size,
                       GoosePassport &passport, GooseState &state);

    /**
     * @function parse_sv_packet
     */
    static int
    parse_sv_packet(const uint8_t *buffer, int size,
                    SVStreamPassport &passport, SVStreamState &state);
};

