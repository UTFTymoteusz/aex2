#include "core/netcore.hpp"

#include "aex/dev/dev.hpp"
#include "aex/dev/net.hpp"
#include "aex/ipc/event.hpp"
#include "aex/mem/circularbuffer.hpp"
#include "aex/mem/smartptr.hpp"
#include "aex/mem/vmem.hpp"
#include "aex/proc/thread.hpp"

#include "core/packet_buffer.hpp"
#include "layer/arp.hpp"
#include "layer/ethernet.hpp"
#include "layer/ipv4.hpp"

#include <new>

namespace AEX::NetProto::NetCore {
    packet_buffer* tx_buffers[64];

    Mem::CircularBuffer* tx_packet_queue;
    IPC::Event           tx_packet_event;

    Mem::CircularBuffer* rx_packet_queue;
    IPC::Event           rx_packet_event;

    Mem::SmartPointer<Proc::Thread> tx_handler_thread;
    Mem::SmartPointer<Proc::Thread> rx_handler_thread;

    Spinlock tx_lock, rx_lock;

    void send_arp(const void* packet, uint16_t len);

    void tx_packet_handler();
    void rx_packet_handler();

    void init() {
        for (size_t i = 0; i < sizeof(tx_buffers) / sizeof(packet_buffer*); i++)
            tx_buffers[i] = new packet_buffer();

        tx_packet_queue = new Mem::CircularBuffer(65536);

        auto tx_thread = new Proc::Thread(nullptr, (void*) tx_packet_handler, 8192, nullptr);

        tx_handler_thread = tx_thread->getSmartPointer();
        tx_handler_thread->start();


        rx_packet_queue = new Mem::CircularBuffer(65536);

        auto rx_thread = new Proc::Thread(nullptr, (void*) rx_packet_handler, 8192, nullptr);

        rx_handler_thread = rx_thread->getSmartPointer();
        rx_handler_thread->start();
    }

    packet_buffer* get_tx_buffer() {
        while (true) {
            for (size_t i = 0; i < sizeof(tx_buffers) / sizeof(packet_buffer*); i++)
                if (tx_buffers[i]->lock.tryAcquire()) {
                    tx_buffers[i]->reset();
                    return tx_buffers[i];
                }

            Proc::Thread::yield();
        }
    }

    void queue_tx_packet(ethertype_t type, const void* data, uint16_t len) {
        tx_lock.acquire();

        while (tx_packet_queue->writeAvailable() < len + 4) {
            tx_lock.release();

            Proc::Thread::yield();

            tx_lock.acquire();
        }

        tx_packet_queue->write(&type, 2);
        tx_packet_queue->write(&len, 2);
        tx_packet_queue->write(data, len);

        tx_packet_event.raise();
        tx_lock.release();
    }

    void queue_rx_packet(int device_id, ethertype_t type, const void* data, uint16_t len) {
        rx_lock.acquire();

        while (rx_packet_queue->writeAvailable() < len + 8) {
            rx_lock.release();

            Proc::Thread::yield();

            rx_lock.acquire();
        }

        rx_packet_queue->write(&device_id, 4);
        rx_packet_queue->write(&type, 2);
        rx_packet_queue->write(&len, 2);
        rx_packet_queue->write(data, len);

        rx_packet_event.raise();
        rx_lock.release();
    }

    // Make seperate threads for each interface pls
    void tx_packet_handler() {
        while (true) {
            tx_lock.acquire();

            if (tx_packet_queue->readAvailable() == 0) {
                tx_packet_event.wait();
                tx_lock.release();

                Proc::Thread::yield();
                continue;
            }

            ethertype_t type;
            uint16_t    len;
            uint8_t     buffer[2048];

            tx_packet_queue->read(&type, 2);
            tx_packet_queue->read(&len, 2);
            tx_packet_queue->read(buffer, len);

            tx_lock.release();

            switch (type) {
            case ethertype_t::ETH_ARP:
                send_arp(buffer, len);
                break;
            case ethertype_t::ETH_IPv4:
                break;
            default:
                break;
            }
        }
    }

    void rx_packet_handler() {
        while (true) {
            rx_lock.acquire();

            if (rx_packet_queue->readAvailable() == 0) {
                rx_packet_event.wait();
                rx_lock.release();

                Proc::Thread::yield();
                continue;
            }

            ethertype_t type;
            int         device_id;
            uint16_t    len;
            uint8_t     buffer[2048];

            rx_packet_queue->read(&device_id, 4);
            rx_packet_queue->read(&type, 2);
            rx_packet_queue->read(&len, 2);
            rx_packet_queue->read(buffer, len);

            rx_lock.release();

            auto net_dev = Dev::get_net_device(device_id);
            if (!net_dev.isValid())
                continue;

            switch (type) {
            case ethertype_t::ETH_ARP:
                ARPLayer::parse(net_dev, buffer, len);
                break;
            case ethertype_t::ETH_IPv4:
                IPv4Layer::parse(net_dev, buffer, len);
                break;
            default:
                break;
            }
        }
    }

    Mem::SmartPointer<Dev::Net> get_interface(Net::mac_addr mac) {
        for (auto iterator = Dev::devices.getIterator(); auto device = iterator.next();) {
            if (device->type != Dev::Device::type_t::NET)
                continue;

            auto net_dev = (Dev::Net*) device;

            if (net_dev->ethernet_mac != mac)
                continue;

            return iterator.get_ptr();
        }

        return Dev::devices.get(-1);
    }

    void send_arp(const void* packet, uint16_t len) {
        auto source_mac = (Net::mac_addr*) ((uint8_t*) packet + sizeof(arp_header));
        auto net_dev    = get_interface(*source_mac);

        if (!net_dev.isValid())
            return;

        auto dest_mac   = (Net::mac_addr*) ((uint8_t*) packet + sizeof(arp_header) + 10);
        auto buffer_try = EthernetLayer::encapsulate(*source_mac, *dest_mac, ethertype_t::ETH_ARP);
        if (!buffer_try.has_value)
            return;

        auto buffer = buffer_try.value;
        memcpy(buffer->alloc(len), packet, len);
        net_dev->send(buffer->get(), buffer->length());

        buffer->release();
    }
}