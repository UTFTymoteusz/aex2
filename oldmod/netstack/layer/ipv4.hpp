#pragma once

#include "aex/dev/netdevice.hpp"
#include "aex/endian.hpp"
#include "aex/errno.hpp"
#include "aex/mem/smartptr.hpp"
#include "aex/net/ipv4.hpp"

#include <stdint.h>


// oh my, fragmentation is gonna be so fun

namespace AEX::NetProto {
    enum ipv4_protocol_t : uint8_t {
        IP_HOPOPT = 0,
        IP_ICMP   = 1,
        IP_IGMP   = 2,
        IP_IPv4   = 4,
        IP_TCP    = 6,
        IP_UDP    = 17,
        IP_GRE    = 47,
    };

    struct ipv4_header {
        uint8_t version : 4;
        uint8_t header_length : 4;

        uint8_t differentiated_services;

        big_endian<uint16_t> total_length;
        big_endian<uint16_t> identification;

        uint8_t  flags : 3;
        uint16_t fragment_offset : 13;

        uint8_t         ttl;
        ipv4_protocol_t protocol;

        big_endian<uint16_t> checksum;

        Net::ipv4_addr source;
        Net::ipv4_addr destination;
    } __attribute__((packed));

    class IPv4Layer {
        public:
        static const Net::ipv4_addr BROADCAST_ADDR;

        static error_t parse(Dev::NetDevice_SP net_dev, const void* packet_ptr, size_t len);

        private:
    };
}