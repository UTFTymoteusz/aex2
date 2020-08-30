#pragma once

#include "aex/byte.hpp"
#include "aex/dev.hpp"
#include "aex/net.hpp"

#include <stdint.h>

namespace AEX::NetStack {
    struct tcp_header {
        big_endian<uint16_t> source_port;
        big_endian<uint16_t> destination_port;

        big_endian<uint32_t> seq_number;
        big_endian<uint32_t> ack_number;

        big_endian<uint16_t> fucking_bitvalues;
        big_endian<uint16_t> window;

        big_endian<uint16_t> checksum;
        big_endian<uint16_t> urgent_pointer;

        uint8_t options[];
    } __attribute__((packed));

    static_assert(sizeof(tcp_header) == 20);

    struct tcp_fake_ipv4_header {
        Net::ipv4_addr       source;
        Net::ipv4_addr       destination;
        uint8_t              zero;
        uint8_t              protocol;
        big_endian<uint16_t> length;
    } __attribute__((packed));

    class TCPLayer {
        public:
        static void parse(Dev::NetDevice_SP net_dev, Net::ipv4_addr source,
                          Net::ipv4_addr destination, uint8_t* buffer, uint16_t len);

        private:
    };
}
