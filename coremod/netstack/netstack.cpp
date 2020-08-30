#include "netstack.hpp"

#include "aex/dev.hpp"
#include "aex/printk.hpp"

#include "layer/link/arp.hpp"
#include "layer/network/ipv4.hpp"

namespace NetStack {
    void arp_received(int device_id, const uint8_t* buffer, uint16_t len) {
        auto dev = AEX::Dev::get_net_device(device_id);
        if (!dev)
            return;

        ARPLayer::parse(dev, buffer, len);
    }

    void ipv4_received(int device_id, const uint8_t* buffer, uint16_t len) {
        auto dev = AEX::Dev::get_net_device(device_id);
        if (!dev)
            return;

        IPv4Layer::parse(dev, buffer, len);
    }

    void ipv6_received(int device_id, const uint8_t* buffer, uint16_t len) {
        AEX::printk("netstack: ipv6 received\n");
    }
}