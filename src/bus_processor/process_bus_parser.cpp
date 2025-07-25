#include "process_bus_parser.hpp"

#include <iostream>

namespace
{
    inline int decode_asn1_len(const uint8_t* buffer, size_t& pos)
    {
        int lenByte = buffer[pos++];
        if (lenByte & 0x80) {
            int lenBytes = lenByte & 0x7F, length = 0;
            for (int i=0;i<lenBytes;++i) {
                length = (length << 8) | buffer[pos++];
            }
            return length;
        }
        return lenByte;
    }

    inline uint32_t decode_asn1_number(const uint8_t* buffer, size_t size)
    {
        switch (size) {
        case 1:
            return buffer[0];
        case 2:
            return (static_cast<uint32_t>(buffer[0]) << 8)
                    | buffer[1];
        case 3:
            return (static_cast<uint32_t>(buffer[0]) << 16)
                    | (static_cast<uint32_t>(buffer[1]) << 8)
                    | buffer[2];
        case 4:
            return (static_cast<uint32_t>(buffer[0]) << 24)
                    | (static_cast<uint32_t>(buffer[1]) << 16)
                    | (static_cast<uint32_t>(buffer[2]) << 8)
                    | buffer[3];
        }
        return 0;
    }

    inline std::string_view make_stringview(const uint8_t* data, int length)
    {
        return {
            reinterpret_cast< const char* >(data),
            static_cast< size_t >(length)
        };
    }
}

#define NET_TO_CPU_U16(x)       RTE_STATIC_BSWAP16(*(uint16_t *)(x))
#define NET_TO_CPU_U32(x)       RTE_STATIC_BSWAP32(*(uint32_t *)(x))
#define NET_TO_CPU_U64(x)       RTE_STATIC_BSWAP64(*(uint64_t *)(x))

int ProcessBusParser::parse_goose_packet(const uint8_t *buffer, int size,
                                         GoosePassport &passport,
                                         GooseState &state)
{
    if (size < 64) {
        return -1;
    }

    passport.dmac = MAC(buffer);

    size_t pos = 14;
    if (buffer[12] == 0x81 && buffer[13] == 0x00 &&
        buffer[16] == 0x88 && buffer[17] == 0xB8) {
        // VLAN -> GOOSE
        pos += 4;
    } else if (buffer[12] == 0x88 && buffer[13] == 0xB8) {
        // GOOSE without VLAN
    } else {
        return -1;
    }

    passport.appid = NET_TO_CPU_U16(buffer + pos);
    pos += 8; // APPID, Length, Reserv1, Reserv2

    // PDU
    if (buffer[pos++] != 0x61) {
        return -3;
    }
    int pduSize = decode_asn1_len(buffer, pos);

    bool found_gocbref = false, found_dataset = false, found_goid = false;
    while (pos < size) {
        uint8_t tag = buffer[pos++];
        int itemSize = decode_asn1_len(buffer, pos);
        if (pos + itemSize > size || itemSize == 0) {
            return -3;
        }

        switch (tag) {
        case 0x80: /* gocbRef */
            passport.gocbref = make_stringview(buffer + pos, itemSize);
            found_gocbref = true;
            break;
        case 0x81: /* timeAllowedToLive */
            break;
        case 0x82: /* DatSet */
            passport.dataset = make_stringview(buffer + pos, itemSize);
            found_dataset = true;
            break;
        case 0x83: /* GoID */
            passport.goid = make_stringview(buffer + pos, itemSize);
            found_goid = true;
            break;
        case 0x84:
            if(itemSize == 4 || itemSize == 6) {
                state.timestamp = NET_TO_CPU_U64(buffer + pos);
            }
            break;
        case 0x85:
            state.stNum = decode_asn1_number(buffer + pos, itemSize);
            break;
        case 0x86:
            state.sqNum = decode_asn1_number(buffer + pos, itemSize);
            break;
        case 0x87: /* Simulation */
            break;
        case 0x88: /* CRev */
            passport.crev = decode_asn1_number(buffer + pos, itemSize);
            break;
        case 0x89: /* NdsCom */
            break;
        case 0x8a: /* Num DataSet entries */
            passport.num = decode_asn1_number(buffer + pos, itemSize);
            break;
        case 0xab: /* allData */
            break;
        case 0x30: // SEQUENCE
        case 0x31: // SET
            for (size_t end = pos + itemSize; pos < end;) {
                uint8_t innerTag = buffer[pos++];
                pos += decode_asn1_len(buffer, pos);
            }
            continue;
        case 0xA0: // Context-specific 0
        case 0xA1: // Context-specific 1
            pos += decode_asn1_len(buffer, pos);
            continue;
        default:
            break;
        }

        pos += itemSize;
    }
    return (found_gocbref && found_dataset && found_goid) ? 0 : -100;
}

int ProcessBusParser::parse_sv_packet(const uint8_t *buffer, int size,
                                      SVStreamPassport &passport,
                                      SVStreamState &state)
{
    if (size < 64) {
        return -1;
    }

    passport.dmac = MAC(buffer);

    size_t pos = 14;
    if (buffer[12] == 0x81 && buffer[13] == 0x00 &&
        buffer[16] == 0x88 && buffer[17] == 0xBA) {
        // VLAN -> SV
        pos += 4;
    } else if (buffer[12] == 0x88 && buffer[13] == 0xBA) {
        // SV without VLAN
    } else {
        return -2;
    }

    passport.appid = NET_TO_CPU_U16(buffer + pos);
    pos += 8; // APPID, Length, Reserv1, Reserv2

    // SV PDU (tag 0x60)
    if (pos >= size || buffer[pos++] != 0x60) {
        return -3;
    }
    ++pos;

    // Parse noASDU (tag 0x80)
    if (pos + 3 <= size && buffer[pos] == 0x80) {
        passport.num =  buffer[pos + 2];
        pos += 3;
    }

    // Sequence of ASDUs (tag 0xa2)
    if (pos >= size || buffer[pos++] != 0xa2) {
        return -4;
    }
    ++pos;

    // ASDU (tag 0x30)
    if (pos >= size || buffer[pos++] != 0x30) {
        return -5;
    }
    ++pos;

    // Parse ASDU fields
    while (pos < size) {
        uint8_t tag = buffer[pos++];
        int length = buffer[pos++]; // Simplified length
        if (pos + length > size) {
            break;
        }

        switch (tag) {
        case 0x80: // svID
            passport.svid =  make_stringview(buffer + pos, length);
            break;
        case 0x82: // smpCnt
            state.smpCnt = NET_TO_CPU_U16(buffer + pos);
            break;
        case 0x83: // confRev
            passport.crev = NET_TO_CPU_U32(buffer + pos);
            break;
        case 0x84: // refrTm
            if (length == 8) {
                /* std::copy(buffer.begin() + pos, buffer.begin() + pos + 8, pkt.refrTm.begin()); */
            }
            break;
        case 0x85: // smpSynch
            /* pkt.smpSynch = buffer[pos]; */
            break;
        case 0x86: // smpRate
            /* pkt.smpRate = (buffer[pos] << 8) | buffer[pos + 1]; */
            break;
        case 0x87: // data
            /* pkt.data = buffer.subspan(pos, length); */
            break;
        case 0x88: // smpMod
            /* pkt.smpMod = buffer[pos]; */
            break;
        }

        pos += length;
    }
    return 0;
}

