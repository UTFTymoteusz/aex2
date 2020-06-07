#include "rx_core.hpp"

#include "aex/dev/dev.hpp"
#include "aex/dev/netdevice.hpp"
#include "aex/ipc/event.hpp"
#include "aex/mem/circularbuffer.hpp"
#include "aex/mem/smartptr.hpp"
#include "aex/spinlock.hpp"

#include "layer/arp.hpp"
#include "layer/ethernet.hpp"
#include "packet_buffer.hpp"

#include <stddef.h>
#include <stdint.h>

namespace AEX::NetStack {
    Mem::CircularBuffer* rx_packet_queue;
    IPC::Event           rx_packet_event;

    Mem::SmartPointer<Proc::Thread> rx_handler_thread;

    Spinlock rx_lock;

    void rx_packet_handler();

    void rx_init() {
        rx_packet_queue = new Mem::CircularBuffer(262144);

        auto rx_thread = new Proc::Thread(nullptr, (void*) rx_packet_handler, 8192, nullptr);

        rx_handler_thread = rx_thread->getSmartPointer();
        rx_handler_thread->start();
    }

    void handle_rx_packet(int device_id, ethertype_t type, uint8_t* buffer, uint16_t len) {
        auto net_dev = Dev::get_net_device(device_id);
        if (!net_dev.isValid())
            return;

        switch (type) {
        case ethertype_t::ETH_ARP:
            ARPLayer::parse(net_dev, buffer, len);
            break;
        /*case ethertype_t::ETH_IPv4:
            IPv4Layer::parse(net_dev, buffer, len);
            break;*/
        default:
            break;
        }
    }

    void queue_rx_packet(int device_id, ethertype_t type, const void* data, uint16_t len) {
        rx_lock.acquire();

        while (rx_packet_queue->writeAvailable() < len + 8) {
            rx_lock.release();

            printk("rx: too much\n");
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

    void rx_packet_handler() {
        while (true) {
            rx_lock.acquire();

            if (rx_packet_queue->readAvailable() == 0) {
                rx_packet_event.wait();
                rx_lock.release();

                Proc::Thread::yield();
                continue;
            }

            int         device_id;
            ethertype_t type;
            uint16_t    len;
            uint8_t     buffer[2048];

            rx_packet_queue->read(&device_id, 4);
            rx_packet_queue->read(&type, 2);
            rx_packet_queue->read(&len, 2);
            rx_packet_queue->read(buffer, len);

            rx_lock.release();

            handle_rx_packet(device_id, type, buffer, len);
        }
    }
}
