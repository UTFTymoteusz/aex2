#include "layer/link/ethernet.hpp"
#include "layer/link/arp.hpp"

#include "aex/byte.hpp"
#include "aex/dev.hpp"
#include "aex/printk.hpp"

#include "rx_core.hpp"
#include "tx_core.hpp"

#include <stddef.h>
#include <stdint.h>

using namespace AEX::Net;

namespace AEX::NetStack {
    error_t EthernetLayer::parse(int device_id, const void* packet_ptr, size_t len) {
        if (len < sizeof(ethernet_header))
            return EINVAL;

        auto header  = (ethernet_header*) packet_ptr;
        auto net_dev = Dev::get_net_device(device_id);

        if (header->destination != net_dev->info.ipv4.mac && !header->destination.isBroadcast())
            return EINVAL;

        //

        return ENONE;
    }

    error_t EthernetLayer::encapsulate(int device_id, packet_buffer* buffer, net_type_t type) {
        auto net_dev = Dev::get_net_device(device_id);
        auto source  = net_dev->info.ipv4.mac;

        auto dest_try = ARPLayer::get_cached()

        auto header = (ethernet_header*) buffer->alloc(sizeof(ethernet_header));

        header->source      = source;
        header->destination = dest;
        header->ethertype   = (uint16_t) type;

        return buffer;
    }
}