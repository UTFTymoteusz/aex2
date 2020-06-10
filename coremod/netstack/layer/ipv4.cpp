#include "layer/ipv4.hpp"

#include "aex/dev/netdevice.hpp"
#include "aex/printk.hpp"

#include "checksum.hpp"
#include "layer/arp.hpp"
#include "layer/none.hpp"
#include "layer/tcp.hpp"
#include "layer/udp.hpp"
#include "tx_core.hpp"

namespace AEX::NetStack {
    void IPv4Layer::parse(Dev::NetDevice_SP net_dev, uint8_t* buffer, uint16_t len) {
        if (len < sizeof(ipv4_header))
            return;

        auto header = (ipv4_header*) buffer;
        auto dst    = header->destination;
        if (dst != net_dev->info.ipv4.addr && dst != BROADCAST &&
            dst != net_dev->info.ipv4.broadcast)
            return;

        uint32_t total = sum_bytes(header, sizeof(ipv4_header));
        if (to_checksum(total) != 0x0000) {
            printk(PRINTK_WARN "ipv4: Got a packet with an invalid checksum\n");
            return;
        }

        buffer += sizeof(ipv4_header);

        len = min<uint16_t>((uint16_t) header->total_length, len);
        len -= sizeof(ipv4_header);

        switch (header->protocol) {
        case ipv4_protocol_t::IPv4_ICMP:
            printk("ipv4: icmp\n");
            break;
        case ipv4_protocol_t::IPv4_TCP:
            TCPLayer::parse(net_dev, header->source, header->destination, buffer, len);
            break;
        case ipv4_protocol_t::IPv4_UDP:
            UDPLayer::parse(net_dev, header->source, header->destination, buffer, len);
            break;
        default:
            printk("ipv4: unknown (%i)\n", header->protocol);
            break;
        }
    }

    optional<packet_buffer*> IPv4Layer::encapsulate(Net::ipv4_addr source, Net::ipv4_addr dest,
                                                    ipv4_protocol_t type, Dev::NetDevice_SP net_dev,
                                                    uint16_t len) {
        packet_buffer* buffer = nullptr;

        switch (net_dev->link_type) {
        case link_type_t::LINK_ETHERNET: {
            mac_addr mac;

            bool local = (net_dev->info.ipv4.addr & net_dev->info.ipv4.mask) ==
                         (dest & net_dev->info.ipv4.mask);

            auto dest_arp  = local ? dest : net_dev->info.ipv4.gateway;
            bool mac_found = false;

            for (int i = 0; i < 6; i++) {
                auto mac_try = ARPLayer::query_ipv4(net_dev, dest_arp);
                if (!mac_try.has_value)
                    continue;

                mac       = mac_try.value;
                mac_found = true;
                break;
            }

            if (!mac_found)
                return local ? error_t::EHOSTDOWN : error_t::EHOSTUNREACH;

            buffer = EthernetLayer::encapsulate(net_dev->info.ipv4.mac, mac, ethertype_t::ETH_IPv4);
        } break;
        default:
            buffer = NoneLayer::encapsulate(ethertype_t::ETH_IPv4);
            break;
        }

        auto header = (ipv4_header*) buffer->alloc(sizeof(ipv4_header));

        header->version                 = 4;
        header->header_length           = 5;
        header->differentiated_services = 0;
        header->total_length            = len + sizeof(ipv4_header);
        header->identification          = 0x2137;
        header->flags                   = 0x4000;
        header->ttl                     = 69;
        header->protocol                = type;
        header->header_checksum         = 0x0000;
        header->source                  = source;
        header->destination             = dest;

        header->header_checksum = to_checksum(sum_bytes(header, sizeof(ipv4_header)));

        return buffer;
    }
}
