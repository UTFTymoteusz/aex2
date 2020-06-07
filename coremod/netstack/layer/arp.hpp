#pragma once

#include "aex/dev/netdevice.hpp"
#include "aex/endian.hpp"
#include "aex/ipc/queryqueue.hpp"
#include "aex/mem/smartptr.hpp"
#include "aex/mem/vector.hpp"
#include "aex/net/ethernet.hpp"
#include "aex/net/ipv4.hpp"
#include "aex/optional.hpp"

#include <stdint.h>

namespace AEX::NetStack {
    enum arp_hardware_type_t {
        ARP_RESERVED = 0,
        ARP_ETHERNET = 1,
    };

    enum arp_opcode_t {
        ARP_REQUEST = 1,
        ARP_REPLY   = 2,
    };

    struct arp_header {
        big_endian<uint16_t> hardware_type;
        big_endian<uint16_t> protocol_type;

        uint8_t hardware_size;
        uint8_t protocol_size;

        big_endian<uint16_t> opcode;
    } __attribute__((packed));

    struct arp_ipv4 {
        Net::mac_addr  sender_mac;
        Net::ipv4_addr sender_ipv4;
        Net::mac_addr  target_mac;
        Net::ipv4_addr target_ipv4;
    } __attribute__((packed));

    struct arp_packet {
        arp_header header;

        union {
            arp_ipv4 ipv4;
        } __attribute__((packed));

        char footer[12];

        arp_packet(){};

        inline bool is_valid_ipv4();
    } __attribute__((packed));

    struct arp_query {
        Net::mac_addr  mac;
        Net::ipv4_addr ipv4;

        arp_query(Net::ipv4_addr _ipv4) {
            ipv4 = _ipv4;
        }
    };

    class ARPLayer {
        public:
        static optional<Net::mac_addr> query_ipv4(Dev::NetDevice_SP net_dev, Net::ipv4_addr addr);
        static void parse(Dev::NetDevice_SP net_dev, uint8_t* buffer, uint16_t len);

        private:
        static IPC::QueryQueue<arp_query, 1000> _query_queue;

        friend void parse_ipv4_reply(Dev::NetDevice_SP net_dev, arp_packet* packet);
    };
} // namespace AEX::NetStack
