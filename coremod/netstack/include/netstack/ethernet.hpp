#pragma once

#include "aex/byte.hpp"         // meh
#include "aex/net/ethernet.hpp" // meh

#include <stdint.h>

namespace NetStack {
    enum ethertype_t : uint16_t {
        ETH_IPv4 = 0x0800,
        ETH_ARP  = 0x0806,
        ETH_IPv6 = 0x08DD,
    };

    struct ethernet_frame {
        AEX::Net::mac_addr        destination;
        AEX::Net::mac_addr        source;
        AEX::big_endian<uint16_t> ethertype;

        uint8_t payload[];
    } __attribute__((packed));
}