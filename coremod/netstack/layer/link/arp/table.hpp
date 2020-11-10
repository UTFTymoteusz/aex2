#pragma once

#include "aex/mem.hpp"
#include "aex/net/ethernet.hpp"
#include "aex/net/ipv4.hpp"
#include "aex/optional.hpp"
#include "aex/proc/thread.hpp"

#include "layer/link/arp/types.hpp"
#include "netstack/arp.hpp"

namespace NetStack {
    class ARPTable {
        public:
        static constexpr auto ARP_REFRESH_NS  = 10ul * 1000000000ul;
        static constexpr auto ARP_TIMEOUT_NS  = 15ul * 1000000000ul;
        static constexpr auto ARP_INTERVAL_NS = 1ul * 1000000000ul;
        static constexpr auto ARP_RETRIES     = 5;

        void init();

        AEX::optional<AEX::Net::mac_addr> get_mac(AEX::Net::ipv4_addr ipv4);

        void set_mac(AEX::Net::ipv4_addr ipv4, AEX::Net::mac_addr mac);

        private:
        struct arp_entry {
            arp_uuid_t uuid;
            uint64_t   set_at;
            bool       updating;

            union {
                AEX::Net::ipv4_addr ipv4;
            };
            union {
                AEX::Net::mac_addr mac;
            };

            arp_entry() {}
        };

        struct arp_query {
            arp_uuid_t uuid;
            uint64_t   retry_at;
            uint32_t   retries;

            union {
                AEX::Net::ipv4_addr ipv4;
            };

            arp_query() {}
        };

        static AEX::Mem::Vector<arp_entry, 16, 16> m_entries;
        static AEX::Spinlock                       m_entries_lock;
        static AEX::Mem::Vector<arp_query, 8, 8>   m_queries;
        static AEX::Spinlock                       m_queries_lock;

        static AEX::Proc::Thread* m_loop_thread;

        static void loop();

        static void query_mac(AEX::Net::ipv4_addr ipv4);
        static void send_mac_request(AEX::Net::ipv4_addr ipv4);
    };
}