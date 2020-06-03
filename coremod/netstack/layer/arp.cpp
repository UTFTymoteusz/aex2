#include "layer/arp.hpp"

#include "aex/byte.hpp"
#include "aex/debug.hpp"
#include "aex/dev/dev.hpp"
#include "aex/dev/device.hpp"
#include "aex/dev/net.hpp"
#include "aex/net/ipv4.hpp"
#include "aex/printk.hpp"

#include "core/netcore.hpp"

#include <stdint.h>


using namespace AEX::Net;

namespace AEX::NetProto {
    void reply_to_ipv4_arp(mac_addr* source_mac, ipv4_addr* source, mac_addr*, ipv4_addr* target) {
        for (auto iterator = Dev::devices.getIterator(); auto device = iterator.next();) {
            if (device->type != Dev::Device::NET)
                continue;

            auto net_dev = (Dev::Net*) device;

            if (*target == net_dev->ipv4_addr) {
                uint8_t buffer[28 + 12];

                ARPLayer::fillLayerIPv4(buffer, arp_header::opcode_t::OP_REPLY,
                                        net_dev->ethernet_mac, *target, *source_mac, *source);

                memcpy(buffer + sizeof(arp_header) + 20, "aexAEXaexAEX", 12);

                NetCore::queue_tx_packet(ethertype_t::ARP, buffer, sizeof(buffer));
                break;
            }
        }
    }

    void ARPLayer::parse(void* packet_ptr, size_t len) {
        if (len < sizeof(arp_header))
            return;

        auto header = (arp_header*) packet_ptr;

        if (fromBigEndian<uint16_t>(header->hardware_type) !=
                arp_header::hardware_type_t::ETHERNET ||
            fromBigEndian<uint16_t>(header->protocol_type) != ethertype_t::IPv4)
            return;

        uint16_t true_len = sizeof(arp_header);

        true_len += header->hardware_size * 2;
        true_len += header->protocol_size * 2;

        if (header->hardware_size != 6 || header->protocol_size != 4 || len < true_len)
            return;

        auto source_mac = (mac_addr*) ((uint8_t*) header + sizeof(header));
        auto source_ip  = (ipv4_addr*) ((uint8_t*) header + sizeof(header) + 6);

        auto target_mac = (mac_addr*) ((uint8_t*) header + sizeof(header) + 10);
        auto target_ip  = (ipv4_addr*) ((uint8_t*) header + sizeof(header) + 16);

        switch (fromBigEndian<uint16_t>(header->opcode)) {
        case arp_header::opcode_t::OP_REQUEST:
            reply_to_ipv4_arp(source_mac, source_ip, target_mac, target_ip);
            break;

        default:
            break;
        }
    }

    void ARPLayer::fillLayerIPv4(void* _buffer, arp_header::opcode_t opcode,
                                 Net::mac_addr source_mac, Net::ipv4_addr source_ip,
                                 Net::mac_addr target_mac, Net::ipv4_addr target_ip) {
        uint8_t* buffer = (uint8_t*) _buffer;

        auto header = (arp_header*) buffer;

        header->hardware_type = (arp_header::hardware_type_t) toBigEndian<uint16_t>(
            arp_header::hardware_type_t::ETHERNET);
        header->protocol_type = (ethertype_t) toBigEndian<uint16_t>(ethertype_t::IPv4);

        header->hardware_size = 6;
        header->protocol_size = 4;

        header->opcode = (arp_header::opcode_t) toBigEndian<uint16_t>((uint16_t) opcode);

        memcpy(buffer + sizeof(arp_header) + 0, &source_mac, 6);
        memcpy(buffer + sizeof(arp_header) + 6, &source_ip, 4);
        memcpy(buffer + sizeof(arp_header) + 10, &target_mac, 6);
        memcpy(buffer + sizeof(arp_header) + 16, &target_ip, 4);
    }
}