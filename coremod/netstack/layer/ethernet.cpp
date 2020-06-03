#include "layer/ethernet.hpp"

#include "aex/byte.hpp"
#include "aex/printk.hpp"

#include "core/netcore.hpp"

#include <stdint.h>

using namespace AEX::Net;

namespace AEX::NetProto {
    error_t EthernetLayer::parse(const void* packet_ptr, size_t len) {
        if (len < sizeof(ethernet_header))
            return error_t::EINVAL;

        auto header = (ethernet_header*) packet_ptr;

        switch (fromBigEndian<uint16_t>(header->ethertype)) {
        case ethertype_t::ARP:
            NetCore::queue_rx_packet(ethertype_t::ARP,
                                     (uint8_t*) packet_ptr + sizeof(ethernet_header),
                                     len - sizeof(ethernet_header));
            break;
        case ethertype_t::IPv4:
            NetCore::queue_rx_packet(ethertype_t::IPv4,
                                     (uint8_t*) packet_ptr + sizeof(ethernet_header),
                                     len - sizeof(ethernet_header));
            break;
        default:
            break;
        }

        return error_t::ENONE;
    }

    optional<NetCore::packet_buffer*> EthernetLayer::encapsulate(mac_addr source, mac_addr dest,
                                                                 ethertype_t type) {
        auto buffer = NetCore::get_tx_buffer();

        auto header = (ethernet_header*) buffer->alloc(sizeof(ethernet_header));

        header->source      = source;
        header->destination = dest;
        header->ethertype   = (ethertype_t) toBigEndian<uint16_t>(type);

        return buffer;
    }
}