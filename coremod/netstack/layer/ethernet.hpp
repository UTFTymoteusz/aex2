#pragma once

#include "aex/net/ethernet.hpp"
#include "aex/net/linklayer.hpp"
#include "aex/optional.hpp"

#include "core/packet_buffer.hpp"

#include <stdint.h>

using namespace AEX::Net;

namespace AEX::NetProto {
    enum ethertype_t : uint16_t {
        ARP  = 0x0806,
        IPv4 = 0x0800,
    };

    struct ethernet_header {
        mac_addr destination;
        mac_addr source;

        ethertype_t ethertype;
    } __attribute__((packed));

    class EthernetLayer : public LinkLayer {
      public:
        error_t parse(const void* packet_ptr, size_t len);

        static optional<NetCore::packet_buffer*> encapsulate(mac_addr source, mac_addr dest,
                                                             ethertype_t type);

      private:
    };
}