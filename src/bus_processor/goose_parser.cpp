#include "goose_parser.hpp"

#include <iostream>

namespace
{
    inline int decodeAsn1TagLength(const uint8_t* buffer)
    {
        return 0;
    }

    inline int decodeAsn1Length(const uint8_t* buffer, size_t& pos)
    {
        int lenByte = buffer[pos++];
        if (lenByte & 0x80) {
            int lenBytes = lenByte & 0x7F;
            int length = 0;
            for (int i = 0; i < lenBytes; ++i) {
                length = (length << 8) | buffer[pos++];
            }
            return length;
        }
        return lenByte;
    }

    template<typename T>
    inline T bytesToNumber(const uint8_t* data, int length)
    {
        T value = 0;
        for (int i=0;i<length;++i) {
            value = (value << 8) | data[i];
        }
        return value;
    }

    inline std::string_view makeStringView(const uint8_t* data, int length)
    {
        return {
            reinterpret_cast< const char* >(data),
            static_cast< size_t >(length)
        };
    }

    inline void skipElement(const uint8_t* buffer, size_t& pos)
    {
        int itemSize = decodeAsn1Length(buffer, pos);
        pos += itemSize;
    }
}

int parse_goose_packet(const uint8_t *buffer, int size,
                       GoosePassport &passport, GooseState &state)
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
    } else if (buffer[12] == 0x88 && buffer[13] == 0xb8) {
        // GOOSE without VLAN
    } else {
        return -1;
    }

    passport.appid = bytesToNumber< uint16_t >(buffer + pos, 2);
    pos += 8; // APPID, Length, Reserv1, Reserv2

    // PDU
    if (buffer[pos++] != 0x61) {
        return -3;
    }
    int pduSize = decodeAsn1Length(buffer, pos);

    bool found_gocbref = false, found_dataset = false, found_goid = false;
    while (pos < size) {
        uint8_t tag = buffer[pos++];
        int itemSize = decodeAsn1Length(buffer, pos);
        if (pos + itemSize > size || itemSize == 0) {
            return -3;
        }

        switch (tag) {
        case 0x80: /* gocbRef */
            passport.gocbref = makeStringView(buffer + pos, itemSize);
            found_gocbref = true;
            break;
        case 0x81: /* timeAllowedToLive */
            break;
        case 0x82: /* DatSet */
            passport.dataset = makeStringView(buffer + pos, itemSize);
            found_dataset = true;
            break;
        case 0x83: /* GoID */
            passport.goid = makeStringView(buffer + pos, itemSize);
            found_goid = true;
            break;
        case 0x84:
            if(itemSize == 4 || itemSize == 6) {
                /* passport.timestamp = bytesToNumber< uint64_t >(buffer + pos, itemSize); */
            }
            break;
        case 0x85:
            state.stNum = bytesToNumber< uint32_t >(buffer + pos, itemSize);
            break;
        case 0x86:
            state.sqNum = bytesToNumber< uint32_t >(buffer + pos, itemSize);
            break;
        case 0x87: /* Simulation */
            break;
        case 0x88: /* CRev */
            passport.crev = bytesToNumber< uint32_t >(buffer + pos, itemSize);
            break;
        case 0x89: /* NdsCom */
            break;
        case 0x8a: /* Num DataSet entries */
            passport.num = bytesToNumber< uint32_t >(buffer + pos, itemSize);
            break;
        case 0xab: /* allData */
            break;
        case 0x30: // SEQUENCE
        case 0x31: // SET
            for (size_t end = pos + itemSize; pos < end;) {
                uint8_t innerTag = buffer[pos++];
                skipElement(buffer, pos);
            }
            continue;
        case 0xA0: // Context-specific 0
        case 0xA1: // Context-specific 1
            skipElement(buffer, pos);
            continue;
        default:
            break;
        }

        pos += itemSize;
    }

    return (found_gocbref && found_dataset && found_goid) ? 0 : -100;
}

bool is_goose(const uint8_t* buffer, int size, uint16_t *appid)
{
    if (buffer[12] == 0x81 && buffer[13] == 0x00 /* VLAN */ &&
        buffer[16] == 0x88 && buffer[17] == 0xB8 /* GOOSE */) {
        // Standard situation
        *appid = bytesToNumber< uint16_t >(buffer + 18, 2);
        return true;
    } else if (buffer[12] == 0x88 && buffer[13] == 0xb8) {
        // GOOSE without VLAN
        *appid = bytesToNumber< uint16_t >(buffer + 14, 2);
        return true;
    } else {
        // It is not a GOOSE
        return false;
    }
}

