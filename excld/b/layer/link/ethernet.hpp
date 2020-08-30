#pragma once

#include "aex/byte.hpp"
#include "aex/net/ethernet.hpp"
#include "aex/net/linklayer.hpp"
#include "aex/net/packet_buffer.hpp"
#include "aex/optional.hpp"

#include <stddef.h>
#include <stdint.h>

using namespace AEX::Net;

namespace AEX::NetStack {
    enum ethertype_t : uint16_t {
        ETH_ARP  = 0x0806,
        ETH_IPv4 = 0x0800,
    };

    struct ethernet_header {
        mac_addr destination;
        mac_addr source;

        big_endian<uint16_t> ethertype;
    } __attribute__((packed));

    class EthernetLayer : public LinkLayer {
        public:
        error_t parse(int device_id, const void* packet_ptr, size_t len);
        error_t encap(int device_id, packet_buffer* buffer_ptr, net_type_t type);

        private:
    };
}