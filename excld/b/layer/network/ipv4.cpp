#include "layer/ipv4.hpp"

#include "aex/dev.hpp"
#include "aex/printk.hpp"
#include "aex/proc.hpp"

#include "checksum.hpp"
#include "common.hpp"
#include "layer/arp.hpp"
#include "layer/none.hpp"
#include "layer/tcp.hpp"
#include "layer/udp.hpp"
#include "tx_core.hpp"

namespace AEX::NetStack {
    Proc::Thread_SP IPv4Layer::_loop_thread;
    IPC::Event      IPv4Layer::_loop_event;
    Spinlock        IPv4Layer::_loop_lock;

    IPv4Layer::ipv4_retry_packet* IPv4Layer::_retry_queue_start;
    IPv4Layer::ipv4_retry_packet* IPv4Layer::_retry_queue_end;
    Spinlock                      IPv4Layer::_retry_queue_lock;

    void IPv4Layer::init() {
        _retry_queue_start = nullptr;
        _retry_queue_end   = nullptr;

        auto thread =
            new Proc::Thread(nullptr, (void*) loop, Proc::Thread::KERNEL_STACK_SIZE, nullptr);
        _loop_thread = thread->getSmartPointer();

        thread->start();
    }

    void IPv4Layer::loop() {
        while (true) {
            _loop_event.wait();
        }
    }

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
        case IPv4_PROT_ICMP:
            printk("ipv4: icmp\n");
            break;
        case IPv4_PROT_TCP:
            TCPLayer::parse(net_dev, header->source, header->destination, buffer, len);
            break;
        case IPv4_PROT_UDP:
            UDPLayer::parse(net_dev, header->source, header->destination, buffer, len);
            break;
        default:
            printk("ipv4: unknown (%i)\n", header->protocol);
            break;
        }
    }

    optional<packet_buffer*> IPv4Layer::encapsulate(Net::ipv4_addr source, Net::ipv4_addr dest,
                                                    ipv4_protocol_t type, uint16_t len) {
        auto buffer = get_tx_buffer();
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

    error_t IPv4Layer::route_tx(packet_buffer* packet, int flags) {
        auto header = (ipv4_header*) packet->get();
        auto sorc   = header->source;
        auto dest   = header->destination;

        auto net_dev = get_interface_for_dst(dest);
        if (!net_dev)
            return EHOSTUNREACH;

        switch (net_dev->link_type) {
        case LINK_ETHERNET: {
            mac_addr mac;

            bool local = net_dev->info.ipv4.addr.isSubnettedWith(dest, net_dev->info.ipv4.mask);

            auto dest_arp = local ? dest : net_dev->info.ipv4.gateway;

            auto mac_try = ARPLayer::get_cached(dest_arp);
            if (!mac_try && (flags & ENCAP_NOBLOCK)) {
                printk("EAGAIN %i (%i.%i.%i.%i)\n", flags, dest[0], dest[1], dest[2], dest[3]);

                for (volatile size_t i = 0; i < 4354353; i++)
                    ;

                return EAGAIN;
            }

            if (!mac_try)
                for (int i = 0; i < 6; i++) {
                    auto mac_try = ARPLayer::query_ipv4(net_dev, dest_arp);
                    if (!mac_try)
                        continue;

                    break;
                }

            if (!mac_try)
                return EHOSTUNREACH;

            // Make the tx buffer move from down to up, so you could just move the pointer back down
            // on failure instead of having to alloc a whole new buffer.
            auto buffer = get_tx_buffer();
            buffer->copy(packet, 0, 0, 1500);

            EthernetLayer::encapsulate(buffer, net_dev->info.ipv4.mac, mac_try.value, ETHTYPE_IPv4);

            net_dev->send(buffer->get(), buffer->length());

            buffer->release();
        } break;
        default:
            break;
        }

        return ENONE;
    }
}
