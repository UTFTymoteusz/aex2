#pragma once

#include "aex/byte.hpp"
#include "aex/dev.hpp"
#include "aex/ipc/queryqueue.hpp"
#include "aex/mem.hpp"
#include "aex/net.hpp"
#include "aex/net/ethernet.hpp"
#include "aex/optional.hpp"
#include "aex/proc.hpp"
#include "aex/spinlock.hpp"

#include <stddef.h>
#include <stdint.h>

namespace AEX::NetStack {
    static constexpr auto ARP_REFRESH_NS = 10ul * 1000000000;
    static constexpr auto ARP_TIMEOUT_NS = 15ul * 1000000000;
    static constexpr auto ARP_RETRIES    = 5;

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

    struct arp_query_async {
        Dev::NetDevice_SP dev;
        Net::ipv4_addr    ipv4;

        void* state;
        void (*callback)(void* state, optional<Net::mac_addr> mac, bool completed_synchronously);
        bool mastering = false;

        int      count         = 0;
        uint64_t retransmit_at = 0;

        arp_query_async(Dev::NetDevice_SP _dev, Net::ipv4_addr _ipv4, void* _state,
                        void (*_callback)(void* state, optional<Net::mac_addr> mac,
                                          bool completed_synchronously)) {
            dev      = _dev;
            ipv4     = _ipv4;
            state    = _state;
            callback = _callback;
        }
    };

    struct arp_table_entry {
        bool     mac_static = false;
        bool     updating   = false;
        uint64_t updated_at;

        Net::mac_addr  mac;
        Net::ipv4_addr ipv4;

        arp_table_entry(uint64_t updated_at, Net::mac_addr mac, Net::ipv4_addr ipv4,
                        bool mac_static = false) {
            this->updated_at = updated_at;
            this->mac        = mac;
            this->ipv4       = ipv4;
            this->mac_static = mac_static;
        }
    };

    class ARPLayer {
        public:
        static void init();

        static optional<Net::mac_addr> query_ipv4(Dev::NetDevice_SP net_dev, Net::ipv4_addr addr);
        static void query_ipv4_callback(Dev::NetDevice_SP net_dev, Net::ipv4_addr addr, void* state,
                                        void (*)(void* state, optional<Net::mac_addr> addr,
                                                 bool completed_synchronously),
                                        bool no_cache = false);

        static void                    cache_result(Net::mac_addr mac_addr, Net::ipv4_addr addr);
        static optional<Net::mac_addr> get_cached(Net::ipv4_addr addr);

        static void add_static_entry(Net::ipv4_addr addr, Net::mac_addr mac);

        static void parse(Dev::NetDevice_SP net_dev, uint8_t* buffer, uint16_t len);

        private:
        static IPC::QueryQueue<arp_query, 1000> _query_queue;
        static Mem::Vector<arp_table_entry>     _arp_table;
        static Spinlock                         _arp_table_lock;
        static Mem::Vector<arp_query_async>     _async_queries;

        static Proc::Thread_SP _loop_thread;
        static IPC::Event      _loop_event;
        static Spinlock        _loop_lock;

        static void loop();

        friend void parse_ipv4_reply(Dev::NetDevice_SP net_dev, arp_packet* packet);
    };
}
