#pragma once

#include "aex/dev/netdevice.hpp"
#include "aex/endian.hpp"
#include "aex/net/ipv4.hpp"

#include <stdint.h>

namespace AEX::NetStack {
    struct udp_header {
        big_endian<uint16_t> source_port;
        big_endian<uint16_t> destination_port;
        big_endian<uint16_t> total_length;
        big_endian<uint16_t> checksum;
    } __attribute__((packed));

    struct udp_fake_ipv4_header {
        Net::ipv4_addr       source;
        Net::ipv4_addr       destination;
        uint8_t              zero;
        uint8_t              protocol;
        big_endian<uint16_t> length;
    } __attribute__((packed));

    class UDPLayer {
        public:
        static void parse(Dev::NetDevice_SP net_dev, Net::ipv4_addr source,
                          Net::ipv4_addr destination, uint8_t* buffer, uint16_t len);

        private:
    };
}
