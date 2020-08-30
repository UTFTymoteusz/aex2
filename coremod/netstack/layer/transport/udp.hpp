#pragma once

#include "aex/byte.hpp"
#include "aex/dev.hpp"
#include "aex/net.hpp"

#include <stdint.h>

namespace NetStack {
    struct udp_header {
        AEX::big_endian<uint16_t> source_port;
        AEX::big_endian<uint16_t> destination_port;
        AEX::big_endian<uint16_t> total_length;
        AEX::big_endian<uint16_t> checksum;
    } __attribute__((packed));

    struct udp_fake_ipv4_header {
        AEX::Net::ipv4_addr       source;
        AEX::Net::ipv4_addr       destination;
        uint8_t                   zero;
        uint8_t                   protocol;
        AEX::big_endian<uint16_t> length;
    } __attribute__((packed));

    class UDPLayer {
        public:
        static void parse(const uint8_t* buffer, uint16_t len, AEX::Net::ipv4_addr src_addr,
                          AEX::Net::ipv4_addr dst_addr);

        private:
    };
}
