#include "table.hpp"

#include "aex/sys/time.hpp"

#include "layer/link/arp.hpp"
#include "layer/network/ipv4.hpp"
#include "netstack/ethernet.hpp"

namespace NetStack {
    AEX::Mem::Vector<ARPTable::arp_entry, 16, 16> ARPTable::m_entries;
    AEX::Spinlock                                 ARPTable::m_entries_lock;
    AEX::Mem::Vector<ARPTable::arp_query, 8, 8>   ARPTable::m_queries;
    AEX::Spinlock                                 ARPTable::m_queries_lock;

    AEX::Proc::Thread* ARPTable::m_loop_thread;

    void ARPTable::init() {
        auto thread   = AEX::Proc::Thread::create(1, (void*) loop,
                                                AEX::Proc::Thread::KERNEL_STACK_SIZE, nullptr);
        m_loop_thread = thread.value;
        m_loop_thread->start();
        m_loop_thread->detach();
    }

    AEX::optional<AEX::Net::mac_addr> ARPTable::get_mac(AEX::Net::ipv4_addr ipv4) {
        AEX::ScopeSpinlock scopeLock(m_entries_lock);
        uint64_t           time = AEX::Sys::Time::uptime();


        for (int i = 0; i < m_entries.count(); i++) {
            auto& entry = m_entries[i];

            if (entry.uuid != to_arp_uuid(ARP_ETHERNET, ETH_IPv4))
                continue;

            if (entry.ipv4 == ipv4) {
                if (time - entry.set_at > ARP_TIMEOUT_NS) {
                    m_queries.erase(i);
                    break;
                }

                if (time - entry.set_at > ARP_REFRESH_NS) {
                    entry.updating = true;
                    query_mac(ipv4);
                }

                return entry.mac;
            }
        }

        query_mac(ipv4);
        return {};
    }

    // need to make it actually look if we've been querying
    void ARPTable::set_mac(AEX::Net::ipv4_addr ipv4, AEX::Net::mac_addr mac) {
        {
            AEX::ScopeSpinlock scopeLock(m_queries_lock);

            for (int i = 0; i < m_queries.count(); i++) {
                auto& query = m_queries[i];

                if (query.uuid != to_arp_uuid(ARP_ETHERNET, ETH_IPv4))
                    continue;

                if (query.ipv4 == ipv4) {
                    m_queries.erase(i);
                    i--;

                    continue;
                }
            }
        }

        AEX::ScopeSpinlock scopeLock(m_entries_lock);

        for (int i = 0; i < m_entries.count(); i++) {
            auto& entry = m_entries[i];

            if (entry.uuid != to_arp_uuid(ARP_ETHERNET, ETH_IPv4))
                continue;

            if (entry.ipv4 != ipv4)
                continue;

            entry.set_at   = AEX::Sys::Time::uptime();
            entry.updating = false;
            entry.mac      = mac;

            return;
        }

        arp_entry entry;

        entry.uuid   = to_arp_uuid(ARP_ETHERNET, ETH_IPv4);
        entry.set_at = AEX::Sys::Time::uptime();

        entry.ipv4 = ipv4;
        entry.mac  = mac;

        m_entries.push(entry);
    }

    void ARPTable::loop() {
        while (true) {
            uint64_t time = AEX::Sys::Time::uptime();

            m_queries_lock.acquire();

            for (int i = 0; i < m_queries.count(); i++) {
                auto& query = m_queries[i];

                if (time < query.retry_at)
                    continue;

                if (query.retries == ARP_RETRIES) {
                    m_queries.erase(i);
                    i--;

                    continue;
                }

                if (query.uuid == to_arp_uuid(ARP_ETHERNET, ETH_IPv4))
                    send_mac_request(query.ipv4);

                query.retry_at = time + ARP_INTERVAL_NS;
                query.retries++;
            }

            m_queries_lock.release();

            AEX::Proc::Thread::sleep(100);
        }
    }

    void ARPTable::query_mac(AEX::Net::ipv4_addr ipv4) {
        m_queries_lock.acquire();

        for (int i = 0; i < m_queries.count(); i++) {
            auto& query = m_queries[i];

            if (query.uuid != to_arp_uuid(ARP_ETHERNET, ETH_IPv4))
                continue;

            if (query.ipv4 == ipv4) {
                m_queries_lock.release();
                return;
            }
        }

        arp_query query;

        query.uuid     = to_arp_uuid(ARP_ETHERNET, ETH_IPv4);
        query.retry_at = 0;
        query.retries  = 0;

        query.ipv4 = ipv4;

        m_queries.push(query);
        m_queries_lock.release();
    }

    void ARPTable::send_mac_request(AEX::Net::ipv4_addr ipv4) {
        arp_packet packet;

        packet.header.hardware_type = ARP_ETHERNET;
        packet.header.protocol_type = ETH_IPv4;

        packet.header.hardware_len = 6;
        packet.header.protocol_len = 4;

        packet.header.operation = ARP_REQUEST;

        auto dev = IPv4Layer::get_interface(ipv4);
        if (!dev)
            return;

        packet.eth_ipv4.source_mac       = dev->info.ipv4.mac;
        packet.eth_ipv4.source_ipv4      = dev->info.ipv4.addr;
        packet.eth_ipv4.destination_ipv4 = ipv4;
        packet.eth_ipv4.destination_mac  = {};

        AEX::memcpy(packet.eth_ipv4.footer, "aexAEXaexAEX", 12);

        dev->send(&packet, sizeof(arp_header) + sizeof(arp_eth_ipv4), AEX::Net::NET_ARP);
    }
}