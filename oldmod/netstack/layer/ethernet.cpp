#include "layer/ethernet.hpp"

#include "aex/endian.hpp"
#include "aex/printk.hpp"

#include "core/netcore.hpp"

#include <stdint.h>

using namespace AEX::Net;

namespace AEX::NetProto {
    error_t EthernetLayer::parse(int device_id, const void* packet_ptr, size_t len) {
        if (len < sizeof(ethernet_header))
            return error_t::EINVAL;

        auto header  = (ethernet_header*) packet_ptr;
        auto net_dev = Dev::get_net_device(device_id);

        if (header->destination != net_dev->ethernet_mac && !header->destination.isBroadcast())
            return error_t::EINVAL;

        switch ((uint16_t) header->ethertype) {
        case ethertype_t::ETH_ARP:
            NetCore::queue_rx_packet(device_id, ethertype_t::ETH_ARP,
                                     (uint8_t*) packet_ptr + sizeof(ethernet_header),
                                     len - sizeof(ethernet_header));
            break;
        case ethertype_t::ETH_IPv4:
            NetCore::queue_rx_packet(device_id, ethertype_t::ETH_IPv4,
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
        header->ethertype   = (uint16_t) type;

        return buffer;
    }
}