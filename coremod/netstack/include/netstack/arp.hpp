#pragma once

#include "aex/byte.hpp"         // meh
#include "aex/net/ethernet.hpp" // meh
#include "aex/net/ipv4.hpp"     // meh
#include "aex/optional.hpp"

#include <stdint.h>

namespace NetStack {
    enum arp_hardware {
        ARP_ETHERNET = 1,
        ARP_AX25     = 3,
    };

    enum arp_operation {
        ARP_REQUEST = 1,
        ARP_REPLY   = 2,
    };

    struct arp_header {
        AEX::big_endian<uint16_t> hardware_type;
        AEX::big_endian<uint16_t> protocol_type;

        uint8_t hardware_len;
        uint8_t protocol_len;

        AEX::big_endian<uint16_t> operation;
    } __attribute__((packed));

    struct arp_eth_ipv4 {
        AEX::Net::mac_addr  source_mac;
        AEX::Net::ipv4_addr source_ipv4;
        AEX::Net::mac_addr  destination_mac;
        AEX::Net::ipv4_addr destination_ipv4;

        char footer[12];
    } __attribute__((packed));

    struct arp_raw {
        uint8_t raw[4];
    } __attribute__((packed));

    struct arp_packet {
        arp_header header;

        union {
            arp_eth_ipv4 eth_ipv4;
            arp_raw      raw;
        } __attribute__((packed));

        arp_packet() {}
    } __attribute__((packed));

    AEX::optional<AEX::Net::mac_addr> arp_get_mac(AEX::Net::ipv4_addr ipv4);
}