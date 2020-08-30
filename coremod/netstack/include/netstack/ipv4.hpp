#pragma once

#include "aex/byte.hpp"
#include "aex/net/ipv4.hpp"

#include <stdint.h>

namespace NetStack {
    enum ipv4_protocol_t {
        IPv4_IPv4 = 4,
        IPv4_TCP  = 6,
        IPv4_UDP  = 17,
    };

    struct ipv4_header {
        uint8_t header_len : 4;
        uint8_t version : 4;

        uint8_t dsf;

        AEX::big_endian<uint16_t> total_len;
        AEX::big_endian<uint16_t> id;

        AEX::big_endian<uint16_t> flags;

        uint8_t ttl;
        uint8_t protocol;

        AEX::big_endian<uint16_t> header_checksum;

        AEX::Net::ipv4_addr source;
        AEX::Net::ipv4_addr destination;
    } __attribute__((packed));

    static constexpr AEX::Net::ipv4_addr IPv4_ANY       = AEX::Net::ipv4_addr(0, 0, 0, 0);
    static constexpr AEX::Net::ipv4_addr IPv4_BROADCAST = AEX::Net::ipv4_addr(255, 255, 255, 255);
}