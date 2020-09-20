#include "ipv4.hpp"

#include "aex/dev.hpp"
#include "aex/printk.hpp"

#include "checksum.hpp"
#include "layer/transport/tcp.hpp"
#include "layer/transport/udp.hpp"

namespace NetStack {
    AEX::Mem::Vector<IPv4Layer::retx_frame, 16, 16> IPv4Layer::m_retx_queue;
    AEX::Spinlock                                   IPv4Layer::m_retx_queue_lock;
    uint32_t                                        IPv4Layer::m_retx_queue_size;

    AEX::Proc::Thread* IPv4Layer::m_loop_thread;

    void IPv4Layer::init() {
        auto thread   = AEX::Proc::Thread::create(1, (void*) loop,
                                                AEX::Proc::Thread::KERNEL_STACK_SIZE, nullptr);
        m_loop_thread = thread.value;
        m_loop_thread->start();
        m_loop_thread->detach();
    }

    int IPv4Layer::encaps_len(int payload_len) {
        return sizeof(ipv4_header) + payload_len;
    }

    uint8_t* IPv4Layer::encapsulate(uint8_t* buffer, uint16_t len, AEX::Net::ipv4_addr dst,
                                    ipv4_protocol_t protocol) {
        auto header = (ipv4_header*) buffer;

        header->header_len = 5;
        header->version    = 4;

        header->dsf = 0;

        header->total_len = len + sizeof(ipv4_header);
        header->id        = 0x2137;

        header->flags = 0x4000;

        header->ttl      = 72;
        header->protocol = protocol;

        // checksum and source will be filled at routing
        header->destination = dst;

        return buffer + sizeof(ipv4_header);
    }

    void IPv4Layer::parse(AEX::Dev::NetDevice_SP dev, const uint8_t* buffer, uint16_t len) {
        auto header     = (ipv4_header*) buffer;
        int  header_len = header->header_len * 4;

        if (header_len < 20 || len < header_len)
            return;

        auto& dst = header->destination;
        if (dst != dev->info.ipv4.addr && dst != IPv4_BROADCAST && dst != dev->info.ipv4.broadcast)
            return;

        uint32_t total = sum_bytes(header, header_len);
        if (to_checksum(total) != 0x0000) {
            AEX::printk(PRINTK_WARN "ipv4: Got a packet with an invalid checksum\n");
            return;
        }

        if (header->total_len < header_len)
            return;

        const uint8_t* payload = buffer + header_len;

        len = AEX::min<uint16_t>((uint16_t) header->total_len, len);
        len -= header_len;

        switch (header->protocol) {
        case IPv4_TCP:
            TCPLayer::parse(payload, len, header->source, header->destination);
            break;
        case IPv4_UDP:
            UDPLayer::parse(payload, len, header->source, header->destination);
            break;
        default:
            break;
        }
    }

    AEX::error_t IPv4Layer::route(const uint8_t* buffer, uint16_t len) {
        auto err = send(buffer, len);
        if (err == AEX::EAGAIN)
            retx_add(buffer, len);

        return err;
    }

    AEX::Dev::NetDevice_SP IPv4Layer::get_interface(AEX::Net::ipv4_addr ipv4_addr) {
        using namespace AEX;

        int best_generic = -1;
        int best_metric  = 23232323;

        for (auto iterator = Dev::devices.getIterator(); auto device = iterator.next();) {
            if (device->type != Dev::DEV_NET)
                continue;

            auto net_dev = (Dev::NetDevice*) device;
            if (net_dev->metric < best_metric) {
                best_metric  = net_dev->metric;
                best_generic = iterator.index();
            }

            if ((net_dev->info.ipv4.addr & net_dev->info.ipv4.mask) !=
                (ipv4_addr & net_dev->info.ipv4.mask))
                continue;

            return iterator.get_ptr();
        }

        return Dev::devices.get(best_generic);
    }

    void IPv4Layer::loop() {
        while (true) {
            retx();
            AEX::Proc::Thread::sleep(50);
        }
    }

    bool IPv4Layer::retx() {
        for (int i = 0; i < m_retx_queue.count(); i++) {
            auto& frame = m_retx_queue[i];
            frame.tries++;

            AEX::printk("retransmitting...\n");
            auto err = send(frame.data, frame.len);

            if (err == AEX::ENONE || frame.tries == MAX_RETX_TRIES) {
                delete[] frame.data;

                m_retx_queue_lock.acquire();
                m_retx_queue_size -= sizeof(retx_frame) + AEX::Mem::Heap::msize_total(frame.len);
                m_retx_queue.erase(i);
                m_retx_queue_lock.release();

                i--;
                continue;
            }
        }

        return m_retx_queue.count() == 0;
    }

    void IPv4Layer::retx_add(const uint8_t* buffer, uint16_t len) {
        if (m_retx_queue_size + sizeof(retx_frame) + AEX::Mem::Heap::msize_total(len) >
            MAX_RETX_QUEUE_SIZE)
            return;

        AEX::printk("retx add\n");

        auto frame = retx_frame();

        frame.data = new uint8_t[len];
        frame.len  = len;

        AEX::memcpy(frame.data, buffer, len);

        m_retx_queue_lock.acquire();
        m_retx_queue_size += sizeof(retx_frame) + AEX::Mem::Heap::msize_total(len);
        m_retx_queue.push(frame);
        m_retx_queue_lock.release();
    }

    AEX::error_t IPv4Layer::send(const uint8_t* buffer, uint16_t len) {
        auto header = (ipv4_header*) buffer;

        auto dev = get_interface(header->destination);
        if (!dev)
            return AEX::ENETUNREACH;

        header->source          = dev->info.ipv4.addr;
        header->header_checksum = 0x0000;

        header->header_checksum = to_checksum(sum_bytes(header, sizeof(ipv4_header)));

        return dev->send(buffer, len, AEX::Net::NET_IPv4);
    }
}