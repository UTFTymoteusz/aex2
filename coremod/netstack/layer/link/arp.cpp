#include "arp.hpp"

#include "aex/printk.hpp"

#include "layer/link/arp/table.hpp"
#include "netstack/arp.hpp"
#include "netstack/ethernet.hpp"

namespace NetStack {
    ARPTable table;

    void ARPLayer::init() {
        table.init();
    }

    void ARPLayer::parse(AEX::Dev::NetDevice_SP dev, const uint8_t* buffer, uint16_t len) {
        auto header = (arp_header*) buffer;
        if (len < sizeof(arp_header) + header->hardware_len + header->protocol_len)
            return;

        auto uuid = to_arp_uuid(header->hardware_type, header->protocol_type);

        switch (uuid) {
        case to_arp_uuid(ARP_ETHERNET, ETH_IPv4):
            handle_eth_ipv4(dev, (arp_packet*) buffer);
            break;
        default:
            break;
        }
    }

    AEX::optional<AEX::Net::mac_addr> ARPLayer::get_mac(AEX::Net::ipv4_addr ipv4) {
        return table.get_mac(ipv4);
    }

    void ARPLayer::handle_eth_ipv4(AEX::Dev::NetDevice_SP dev, arp_packet* packet) {
        auto& header = packet->header;

        if (header.hardware_len != 6 || header.protocol_len != 4)
            return;

        uint16_t op = (uint16_t) header.operation;

        if (op == ARP_REQUEST) {
            if (dev->info.ipv4.addr != packet->eth_ipv4.destination_ipv4)
                return;

            auto&      org_packet = packet;
            arp_packet packet     = *org_packet;

            packet.header.operation = ARP_REPLY;

            packet.eth_ipv4.source_ipv4 = dev->info.ipv4.addr;
            packet.eth_ipv4.source_mac  = dev->info.ipv4.mac;

            packet.eth_ipv4.destination_ipv4 = org_packet->eth_ipv4.source_ipv4;
            packet.eth_ipv4.destination_mac  = org_packet->eth_ipv4.source_mac;

            AEX::memcpy(packet.eth_ipv4.footer, "aexAEXaexAEX", 12);

            dev->send(&packet, sizeof(arp_header) + sizeof(arp_eth_ipv4), AEX::Net::NET_ARP);
        }
        else if (op == ARP_REPLY)
            table.set_mac(packet->eth_ipv4.source_ipv4, packet->eth_ipv4.source_mac);
    }
}