#include "layer/arp.hpp"

#include "aex/net/ethernet.hpp"

#include "layer/ethernet.hpp"
#include "tx_core.hpp"

namespace AEX::NetStack {
    IPC::QueryQueue<arp_query, 1000> ARPLayer::_query_queue;

    bool arp_packet::is_valid_ipv4() {
        // clang-format off
        if ((uint16_t) header.hardware_type != arp_hardware_type_t::ARP_ETHERNET ||
            (uint16_t) header.protocol_type != ethertype_t::ETH_IPv4 || 
            header.hardware_size != 6 || header.protocol_size != 4)
            return false;
        // clang-format on

        return true;
    }

    inline void parse_ipv4_request(Dev::NetDevice_SP net_dev, arp_packet* packet) {
        if (packet->ipv4.target_ipv4 != net_dev->ipv4_addr)
            return;

        arp_packet reply_packet;

        reply_packet.header.hardware_type = arp_hardware_type_t::ARP_ETHERNET;
        reply_packet.header.hardware_size = 6;
        reply_packet.header.protocol_type = ethertype_t::ETH_IPv4;
        reply_packet.header.protocol_size = 4;
        reply_packet.header.opcode        = arp_opcode_t::ARP_REPLY;

        reply_packet.ipv4.sender_mac  = net_dev->ethernet_mac;
        reply_packet.ipv4.sender_ipv4 = net_dev->ipv4_addr;
        reply_packet.ipv4.target_mac  = packet->ipv4.sender_mac;
        reply_packet.ipv4.target_ipv4 = packet->ipv4.sender_ipv4;

        memcpy(reply_packet.footer, "aexAEXaexAEX", 12);

        auto eth_buffer = EthernetLayer::encapsulate(net_dev->ethernet_mac, packet->ipv4.sender_mac,
                                                     ethertype_t::ETH_ARP);

        eth_buffer->write(&reply_packet, sizeof(arp_header) + sizeof(arp_ipv4) + 12);
        queue_tx_packet(net_dev, eth_buffer->get(), eth_buffer->length());

        eth_buffer->release();
    }

    inline void parse_ipv4_reply(Dev::NetDevice_SP net_dev, arp_packet* packet) {
        if (packet->ipv4.target_ipv4 != net_dev->ipv4_addr)
            return;

        for (auto iterator = ARPLayer::_query_queue.getIterator(); auto query = iterator.next();) {
            if (query->base->ipv4 != packet->ipv4.sender_ipv4)
                continue;

            query->base->mac = packet->ipv4.sender_mac;
            query->notify_of_success();
        }
    }

    optional<Net::mac_addr> ARPLayer::query_ipv4(Dev::NetDevice_SP net_dev, Net::ipv4_addr addr) {
        auto packet = arp_packet();

        packet.header.hardware_type = arp_hardware_type_t::ARP_ETHERNET;
        packet.header.hardware_size = 6;
        packet.header.protocol_type = ethertype_t::ETH_IPv4;
        packet.header.protocol_size = 4;
        packet.header.opcode        = arp_opcode_t::ARP_REQUEST;

        packet.ipv4.sender_mac  = net_dev->ethernet_mac;
        packet.ipv4.sender_ipv4 = net_dev->ipv4_addr;
        packet.ipv4.target_mac  = mac_addr(0, 0, 0, 0, 0, 0);
        packet.ipv4.target_ipv4 = addr;

        memcpy(packet.footer, "aexAEXaexAEX", 12);

        auto eth_buffer = EthernetLayer::encapsulate(net_dev->ethernet_mac,
                                                     mac_addr(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF),
                                                     ethertype_t::ETH_ARP);
        eth_buffer->write(&packet, sizeof(arp_header) + sizeof(arp_ipv4) + 12);

        // This here is a lazy way to get a thread into being critical
        Spinlock lockyboi;
        lockyboi.acquire();

        arp_query _arp_query = arp_query(addr);

        auto promise = _query_queue.startQuery(&_arp_query);

        queue_tx_packet(net_dev, eth_buffer->get(), eth_buffer->length());

        eth_buffer->release();

        lockyboi.release();

        Proc::Thread::yield();

        if (!promise->success)
            return error_t::EHOSTUNREACH;

        return _arp_query.mac;
    }

    void ARPLayer::parse(Dev::NetDevice_SP net_dev, uint8_t* buffer, uint16_t len) {
        auto packet = (arp_packet*) buffer;

        if ((uint16_t) packet->header.hardware_type != arp_hardware_type_t::ARP_ETHERNET ||
            (uint16_t) packet->header.protocol_type != ethertype_t::ETH_IPv4)
            return;

        uint16_t true_len = sizeof(arp_header);

        true_len += packet->header.hardware_size * 2;
        true_len += packet->header.protocol_size * 2;

        if (len < true_len)
            return;

        if ((uint16_t) packet->header.opcode == arp_opcode_t::ARP_REQUEST) {
            if (packet->is_valid_ipv4())
                parse_ipv4_request(net_dev, packet);
        }
        else if ((uint16_t) packet->header.opcode == arp_opcode_t::ARP_REPLY) {
            if (packet->is_valid_ipv4())
                parse_ipv4_reply(net_dev, packet);
        }
    }
}