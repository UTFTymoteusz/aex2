#pragma once

#include "aex/dev/netdevice.hpp"
#include "aex/endian.hpp"
#include "aex/net/ipv4.hpp"

#include "packet_buffer.hpp"

#include <stdint.h>

namespace AEX::NetStack {
    enum ipv4_protocol_t {
        IPv4_ICMP = 1,
        IPv4_IGMP = 2,
        IPv4_IPv4 = 4,
        IPv4_TCP  = 6,
        IPv4_UDP  = 17,
        IPv4_GRE  = 47,
    };

    struct ipv4_header {
        uint8_t header_length : 4;
        uint8_t version : 4;

        uint8_t differentiated_services;

        big_endian<uint16_t> total_length;
        big_endian<uint16_t> identification;

        big_endian<uint16_t> flags;

        uint8_t ttl;
        uint8_t protocol;

        big_endian<uint16_t> header_checksum;

        Net::ipv4_addr source;
        Net::ipv4_addr destination;
    } __attribute__((packed));

    class IPv4Layer {
        public:
        static constexpr Net::ipv4_addr ANY       = Net::ipv4_addr(0, 0, 0, 0);
        static constexpr Net::ipv4_addr BROADCAST = Net::ipv4_addr(255, 255, 255, 255);

        static void parse(Dev::NetDevice_SP net_dev, uint8_t* buffer, uint16_t len);

        static optional<packet_buffer*> encapsulate(Net::ipv4_addr source, Net::ipv4_addr dest,
                                                    ipv4_protocol_t type, Dev::NetDevice_SP net_dev,
                                                    uint16_t len);

        private:
    };
}
