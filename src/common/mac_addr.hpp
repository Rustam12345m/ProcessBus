#pragma once

#include <stdint.h>
#include <array>
#include <stdexcept>
#include <format>

/**
 * @class MAC
 * @brief Representation of Ethernet MAC-address
 */
class MAC
{
    static constexpr size_t SIZE = 6;
public:
    MAC() {}
    explicit MAC(const uint8_t bytes[SIZE]) {
        std::copy(bytes, bytes + SIZE, m_mac.begin());
    }
    explicit MAC(const std::string_view str) {
        if (str.size() != 17) {
            throw std::invalid_argument("Invalid MAC address length");
        }

        int b[6];
        if (sscanf(str.data(), "%2x:%2x:%2x:%2x:%2x:%2x", 
                   &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) {
            throw std::invalid_argument("Invalid MAC address format");
        }

        for (int i=0;i<6;++i) {
            if (b[i] < 0 || b[i] > 0xFF) {
                throw std::invalid_argument("Invalid byte value");
            }
            m_mac[i] = static_cast<uint8_t>(b[i]);
        }
    }

    std::string toString(char separator = ':') const {
        return std::format("{:02x}{:c}{:02x}{:c}{:02x}{:c}{:02x}{:c}{:02x}{:c}{:02x}",
                           m_mac[0], separator, m_mac[1], separator, m_mac[2], separator,
                           m_mac[3], separator, m_mac[4], separator, m_mac[5]);
    }

    uint64_t toU64() const {
        return (static_cast<uint64_t>(m_mac[0]) << 40) |
               (static_cast<uint64_t>(m_mac[1]) << 32) |
               (static_cast<uint64_t>(m_mac[2]) << 24) |
               (static_cast<uint64_t>(m_mac[3]) << 16) |
               (static_cast<uint64_t>(m_mac[4]) << 8) |
               static_cast<uint64_t>(m_mac[5]);
    }

    bool operator==(const MAC &other) const {
        return m_mac == other.m_mac;
    }
    bool operator!=(const MAC &other) const {
        return m_mac != other.m_mac;
    }

    friend std::ostream& operator<<(std::ostream &os, const MAC &mac) {
        return os << mac.toString();
    }

    const uint8_t* data() const { return m_mac.data(); }
    uint8_t* data() { return m_mac.data(); }

private:
    std::array<uint8_t, SIZE> m_mac = { 0 };
};

