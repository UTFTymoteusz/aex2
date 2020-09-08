#pragma once

#include "aex/net/ipv4.hpp"

#include <stdint.h>

namespace NetStack {
    struct tcp_header {
        AEX::big_endian<uint16_t> source_port;
        AEX::big_endian<uint16_t> destination_port;

        AEX::big_endian<uint32_t> seq_number;
        AEX::big_endian<uint32_t> ack_number;

        AEX::big_endian<uint16_t> goddamned_bitvalues;
        AEX::big_endian<uint16_t> window;

        AEX::big_endian<uint16_t> checksum;
        AEX::big_endian<uint16_t> urgent_pointer;

        uint8_t options[];
    } __attribute__((packed));

    static_assert(sizeof(tcp_header) == 20);

    struct tcp_fake_ipv4_header {
        AEX::Net::ipv4_addr       source;
        AEX::Net::ipv4_addr       destination;
        uint8_t                   zero;
        uint8_t                   protocol;
        AEX::big_endian<uint16_t> length;
    } __attribute__((packed));

    class TCPLayer {
        public:
        static void init();

        static void parse(const uint8_t* buffer, uint16_t len, AEX::Net::ipv4_addr src_addr,
                          AEX::Net::ipv4_addr dst_addr);

        private:
    };
}