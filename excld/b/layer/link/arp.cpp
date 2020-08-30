#include "layer/arp.hpp"

#include "aex/net/ethernet.hpp"
#include "aex/printk.hpp"
#include "aex/proc.hpp"
#include "aex/sys/time.hpp"

#include "layer/ethernet.hpp"
#include "tx_core.hpp"

namespace AEX::NetStack {
    IPC::QueryQueue<arp_query, 1000> ARPLayer::_query_queue;
    Mem::Vector<arp_table_entry>     ARPLayer::_arp_table;
    Spinlock                         ARPLayer::_arp_table_lock;
    Mem::Vector<arp_query_async>     ARPLayer::_async_queries;

    Proc::Thread_SP ARPLayer::_loop_thread = {};
    IPC::Event      ARPLayer::_loop_event  = {};
    Spinlock        ARPLayer::_loop_lock   = {};

    bool arp_packet::is_valid_ipv4() {
        // clang-format off
        if ((uint16_t) header.hardware_type != arp_hardware_type_t::ARP_ETHERNET ||
            (uint16_t) header.protocol_type != ETHTYPE_IPv4 || 
            header.hardware_size != 6 || header.protocol_size != 4)
            return false;
        // clang-format on

        return true;
    }

    inline void parse_ipv4_request(Dev::NetDevice_SP net_dev, arp_packet* packet) {
        if (packet->ipv4.target_ipv4 != net_dev->info.ipv4.addr)
            return;

        arp_packet reply_packet;

        reply_packet.header.hardware_type = arp_hardware_type_t::ARP_ETHERNET;
        reply_packet.header.hardware_size = 6;
        reply_packet.header.protocol_type = ETHTYPE_IPv4;
        reply_packet.header.protocol_size = 4;
        reply_packet.header.opcode        = arp_opcode_t::ARP_REPLY;

        reply_packet.ipv4.sender_mac  = net_dev->info.ipv4.mac;
        reply_packet.ipv4.sender_ipv4 = net_dev->info.ipv4.addr;
        reply_packet.ipv4.target_mac  = packet->ipv4.sender_mac;
        reply_packet.ipv4.target_ipv4 = packet->ipv4.sender_ipv4;

        memcpy(reply_packet.footer, "aexAEXaexAEX", 12);

        auto eth_buffer = EthernetLayer::encapsulate(net_dev->info.ipv4.mac,
                                                     packet->ipv4.sender_mac, ETHTYPE_ARP);

        eth_buffer->write(&reply_packet, sizeof(arp_header) + sizeof(arp_ipv4) + 12);
        queue_tx_packet(net_dev, eth_buffer->get(), eth_buffer->length());

        eth_buffer->release();
    }

    inline void parse_ipv4_reply(Dev::NetDevice_SP net_dev, arp_packet* packet) {
        if (packet->ipv4.target_ipv4 != net_dev->info.ipv4.addr)
            return;

        ARPLayer::cache_result(packet->ipv4.sender_mac, packet->ipv4.sender_ipv4);

        for (auto iterator = ARPLayer::_query_queue.getIterator(); auto query = iterator.next();) {
            if (query->base->ipv4 != packet->ipv4.sender_ipv4)
                continue;

            query->base->mac = packet->ipv4.sender_mac;
            query->notify_of_success();
        }

        ARPLayer::_loop_lock.acquire();

        int count = ARPLayer::_async_queries.count();

        for (int i = 0; i < count; i++) {
            auto query = ARPLayer::_async_queries[i];
            if (packet->ipv4.sender_ipv4 != query.ipv4)
                continue;

            ARPLayer::_async_queries.erase(i);

            query.callback(query.state, packet->ipv4.sender_mac, false);

            count--;
            i--;
        }

        ARPLayer::_loop_lock.release();
    }

    inline void send_arp_request_packet(Dev::NetDevice_SP net_dev, Net::ipv4_addr addr) {
        auto packet = arp_packet();

        packet.header.hardware_type = arp_hardware_type_t::ARP_ETHERNET;
        packet.header.hardware_size = 6;
        packet.header.protocol_type = ETHTYPE_IPv4;
        packet.header.protocol_size = 4;
        packet.header.opcode        = arp_opcode_t::ARP_REQUEST;

        packet.ipv4.sender_mac  = net_dev->info.ipv4.mac;
        packet.ipv4.sender_ipv4 = net_dev->info.ipv4.addr;
        packet.ipv4.target_mac  = mac_addr(0, 0, 0, 0, 0, 0);
        packet.ipv4.target_ipv4 = addr;

        memcpy(packet.footer, "aexAEXaexAEX", 12);

        auto eth_buffer = EthernetLayer::encapsulate(
            net_dev->info.ipv4.mac, mac_addr(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF), ETHTYPE_ARP);
        eth_buffer->write(&packet, sizeof(arp_header) + sizeof(arp_ipv4) + 12);

        // This here is a lazy way to get a thread into being critical
        Spinlock lockyboi;
        lockyboi.acquire();

        queue_tx_packet(net_dev, eth_buffer->get(), eth_buffer->length());

        eth_buffer->release();

        lockyboi.release();
    }

    void bong() {
        Proc::Thread::sleep(1500);

        while (true) {
            Proc::Thread::sleep(500);

            auto dev_try = get_interface_for_dst(ipv4_addr(192, 168, 0, 1));
            if (!dev_try)
                return;

            ARPLayer::query_ipv4_callback(
                dev_try, ipv4_addr(192, 168, 0, 1), nullptr, [](auto, auto addr, auto) {
                    if (addr) {
                        auto mac = addr.value;

                        printk("it works! nyaa~~! %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1],
                               mac[2], mac[3], mac[4], mac[5]);
                    }
                    else
                        printk("not working :(\n");
                });
        }
    }

    void ARPLayer::init() {
        auto thread = new Proc::Thread(nullptr, (void*) loop, 16384, nullptr);

        _loop_thread = thread->getSmartPointer();
        _loop_thread->start();
    }

    // Need to make the mastering thingy way less stupid
    void ARPLayer::loop() {
        while (true) {
            _loop_lock.acquire();

            uint64_t uptime = Sys::get_uptime();
            int      count  = _async_queries.count();

            for (int i = 0; i < count; i++) {
                auto query = _async_queries[i];
                if (query.retransmit_at > uptime)
                    continue;

                if (_async_queries[i].count == ARP_RETRIES) {
                    _async_queries.erase(i);

                    query.callback(query.state, {}, false);

                    count--;
                    i--;

                    continue;
                }

                _async_queries[i].retransmit_at = uptime + 1000000000;
                _async_queries[i].count++;

                if (query.mastering)
                    send_arp_request_packet(query.dev, query.ipv4);
            }

            if (count == 0)
                _loop_event.wait();

            _loop_lock.release();

            if (count == 0)
                Proc::Thread::yield();
            else
                Proc::Thread::sleep(50);
        }
    }

    // I need to make this async, somehow
    optional<Net::mac_addr> ARPLayer::query_ipv4(Dev::NetDevice_SP net_dev, Net::ipv4_addr addr) {
        auto mac_try = get_cached(addr);
        if (mac_try)
            return mac_try.value;

        auto packet = arp_packet();

        packet.header.hardware_type = arp_hardware_type_t::ARP_ETHERNET;
        packet.header.hardware_size = 6;
        packet.header.protocol_type = ETHTYPE_IPv4;
        packet.header.protocol_size = 4;
        packet.header.opcode        = arp_opcode_t::ARP_REQUEST;

        packet.ipv4.sender_mac  = net_dev->info.ipv4.mac;
        packet.ipv4.sender_ipv4 = net_dev->info.ipv4.addr;
        packet.ipv4.target_mac  = mac_addr(0, 0, 0, 0, 0, 0);
        packet.ipv4.target_ipv4 = addr;

        memcpy(packet.footer, "aexAEXaexAEX", 12);

        auto eth_buffer = EthernetLayer::encapsulate(
            net_dev->info.ipv4.mac, mac_addr(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF), ETHTYPE_ARP);
        eth_buffer->write(&packet, sizeof(arp_header) + sizeof(arp_ipv4) + 12);

        // This here is a lazy way to get a thread into being critical
        Spinlock lockyboi;
        lockyboi.acquire();

        arp_query _arp_query = arp_query(addr);
        auto      _promise   = _query_queue.startQuery(&_arp_query);

        queue_tx_packet(net_dev, eth_buffer->get(), eth_buffer->length());

        eth_buffer->release();
        lockyboi.release();

        Proc::Thread::yield();

        if (!_promise->success)
            return EHOSTUNREACH;

        cache_result(_arp_query.mac, addr);

        return _arp_query.mac;
    }

    void ARPLayer::query_ipv4_callback(Dev::NetDevice_SP net_dev, Net::ipv4_addr addr, void* state,
                                       void (*callback)(void* state, optional<Net::mac_addr> addr,
                                                        bool completed_synchronously),
                                       bool no_cache) {
        if (!no_cache) {
            auto mac_try = get_cached(addr);
            if (mac_try) {
                callback(state, mac_try.value, true);
                return;
            }
        }

        auto query     = arp_query_async(net_dev, addr, state, callback);
        auto scopeLock = ScopeSpinlock(_loop_lock);

        query.mastering = true;

        int count = _async_queries.count();

        for (int i = 0; i < count; i++) {
            auto ruery = _async_queries[i];
            if (ruery.ipv4 != addr || !ruery.mastering)
                continue;

            query.mastering = false;
            break;
        }

        query.retransmit_at = 0;

        _async_queries.pushBack(query);

        _loop_event.raise();
    }

    void ARPLayer::cache_result(Net::mac_addr mac_addr, Net::ipv4_addr addr) {
        auto scopeLock = ScopeSpinlock(_arp_table_lock);

        uint64_t uptime = Sys::get_uptime();

        for (int i = 0; i < _arp_table.count(); i++) {
            if (_arp_table[i].ipv4 != addr)
                continue;

            _arp_table[i].mac        = mac_addr;
            _arp_table[i].updated_at = uptime;
            _arp_table[i].updating   = false;

            return;
        }

        _arp_table.pushBack(arp_table_entry(uptime, mac_addr, addr));
    }

    optional<Net::mac_addr> ARPLayer::get_cached(Net::ipv4_addr addr) {
        uint64_t uptime = Sys::get_uptime();

        auto scopeLock = ScopeSpinlock(_arp_table_lock);

        for (int i = 0; i < _arp_table.count(); i++) {
            if (_arp_table[i].ipv4 != addr)
                continue;

            if (_arp_table[i].updated_at + ARP_TIMEOUT_NS <= uptime) {
                _arp_table.erase(i);
                return {};
            }

            if (_arp_table[i].updated_at + ARP_REFRESH_NS <= uptime && !_arp_table[i].updating) {
                _arp_table[i].updating = true;

                auto dev = get_interface_for_dst(addr);
                if (dev)
                    query_ipv4_callback(
                        dev, _arp_table[i].ipv4, nullptr, [](auto, auto, auto) {}, true);

                return _arp_table[i].mac;
            }

            return _arp_table[i].mac;
        }

        return {};
    }

    void ARPLayer::add_static_entry(Net::ipv4_addr addr, Net::mac_addr mac) {
        _arp_table_lock.acquire();

        for (int i = 0; i < _arp_table.count(); i++) {
            if (_arp_table[i].ipv4 != addr)
                continue;

            _arp_table[i].mac_static = true;
            _arp_table[i].mac        = mac;

            _arp_table_lock.release();
            break;
        }

        _arp_table.pushBack(arp_table_entry(0, mac, addr, true));

        _arp_table_lock.release();
    }

    void ARPLayer::parse(Dev::NetDevice_SP net_dev, uint8_t* buffer, uint16_t len) {
        auto packet = (arp_packet*) buffer;

        if ((uint16_t) packet->header.hardware_type != arp_hardware_type_t::ARP_ETHERNET ||
            (uint16_t) packet->header.protocol_type != ETHTYPE_IPv4)
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