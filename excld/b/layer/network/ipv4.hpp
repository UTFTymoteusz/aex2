#pragma once

#include "aex/byte.hpp"
#include "aex/dev.hpp"
#include "aex/net.hpp"

#include <stdint.h>

namespace AEX::NetStack {
    constexpr auto RETRY_QUEUE_SIZE = 65536;

    enum ipv4_protocol_t {
        IPv4_PROT_ICMP = 1,
        IPv4_PROT_IGMP = 2,
        IPv4_PROT_IPv4 = 4,
        IPv4_PROT_TCP  = 6,
        IPv4_PROT_UDP  = 17,
        IPv4_PROT_GRE  = 47,
    };

    struct ipv4_header {
        uint8_t header_length : 4;
        uint8_t version : 4;

        uint8_t differentiated_services;

        big_endian<uint16_t> total_length;
        big_endian<uint16_t> identification;

        big_endian<uint16_t> flags;

        uint8_t ttl;
        uint8_t protocol;

        big_endian<uint16_t> header_checksum;

        Net::ipv4_addr source;
        Net::ipv4_addr destination;
    } __attribute__((packed));

    class IPv4Layer {
        public:
        static constexpr Net::ipv4_addr ANY       = Net::ipv4_addr(0, 0, 0, 0);
        static constexpr Net::ipv4_addr BROADCAST = Net::ipv4_addr(255, 255, 255, 255);

        static void init();
        static void loop();

        static void parse(Dev::NetDevice_SP net_dev, uint8_t* buffer, uint16_t len);

        static optional<Net::packet_buffer*> encapsulate(Net::ipv4_addr source, Net::ipv4_addr dest,
                                                         ipv4_protocol_t type, uint16_t len);

        static error_t route_tx(Net::packet_buffer* packet, int flags = 0);

        private:
        struct ipv4_retry_packet {
            int     len = 0;
            uint8_t data[];
        };

        struct ipv4_queue_head {
            Net::ipv4_addr addr;

            int failures = 0;

            // have a total cap of like 8192 maybe
            Mem::Vector<ipv4_retry_packet*> queue;
            Spinlock                        lock;
        };

        static Proc::Thread_SP _loop_thread;
        static IPC::Event      _loop_event;
        static Spinlock        _loop_lock;
    };
}
