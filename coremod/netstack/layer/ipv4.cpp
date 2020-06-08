#include "layer/ipv4.hpp"

#include "aex/printk.hpp"

#include "checksum.hpp"
#include "layer/udp.hpp"

namespace AEX::NetStack {
    void IPv4Layer::parse(Dev::NetDevice_SP net_dev, uint8_t* buffer, uint16_t len) {
        if (len < sizeof(ipv4_header))
            return;

        auto header = (ipv4_header*) buffer;

        if (header->destination != net_dev->ipv4_addr && header->destination != BROADCAST &&
            header->destination != net_dev->ipv4_broadcast)
            return;

        uint32_t total = sum_bytes(header, sizeof(ipv4_header));
        if (to_checksum(total) != 0x0000) {
            printk(PRINTK_WARN "ipv4: Got a packet with an invalid checksum\n");
            return;
        }

        buffer += sizeof(ipv4_header);
        len -= sizeof(ipv4_header);

        switch (header->protocol) {
        case ipv4_protocol_t::IPv4_ICMP:
            printk("ipv4: icmp\n");
            break;
        case ipv4_protocol_t::IPv4_TCP:
            printk("ipv4: tcp\n");
            break;
        case ipv4_protocol_t::IPv4_UDP:
            UDPLayer::parse(net_dev, header->source, header->destination, buffer, len);
            break;
        default:
            printk("ipv4: unknown (%i)\n", header->protocol);
            break;
        }
    }
}
