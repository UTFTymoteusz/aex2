#pragma once

#include "aex/dev/net.hpp"
#include "aex/mem/smartptr.hpp"
#include "aex/net/ethernet.hpp"
#include "aex/net/ipv4.hpp"

#include "layer/ethernet.hpp"

#include <stdint.h>

namespace AEX::NetProto {
    struct arp_header {
        enum hardware_type_t : uint16_t {
            RESERVED = 0,
            ETHERNET = 1,
        };

        enum opcode_t : uint16_t {
            OP_RESERVED = 0,
            OP_REQUEST  = 1,
            OP_REPLY    = 2,
        };

        big_endian<uint16_t> hardware_type;
        big_endian<uint16_t> protocol_type;

        uint8_t hardware_size;
        uint8_t protocol_size;

        big_endian<uint16_t> opcode;
    } __attribute__((packed));

    class ARPLayer {
      public:
        static void parse(Mem::SmartPointer<Dev::Net> net_dev, void* packet_ptr, size_t len);

        static void fillLayerIPv4(void* buffer, arp_header::opcode_t opcode,
                                  Net::mac_addr source_mac, Net::ipv4_addr source_ip,
                                  Net::mac_addr target_mac, Net::ipv4_addr target_ip);

      private:
    };
}