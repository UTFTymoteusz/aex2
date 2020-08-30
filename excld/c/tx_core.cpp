#include "tx_core.hpp"

#include "aex/dev.hpp"
#include "aex/ipc/event.hpp"
#include "aex/mem.hpp"
#include "aex/printk.hpp"
#include "aex/spinlock.hpp"

#include <stddef.h>
#include <stdint.h>

using namespace AEX;

namespace NetStack {
    Mem::CircularBuffer* tx_packet_queue;
    IPC::Event           tx_packet_event;

    Mem::SmartPointer<Proc::Thread> tx_handler_thread;

    Spinlock tx_lock;

    void tx_packet_handler();

    void tx_init() {
        tx_packet_queue = new Mem::CircularBuffer(65536);

        // auto tx_thread = new Proc::Thread(nullptr, (void*) tx_packet_handler, 8192, nullptr);

        // tx_handler_thread = tx_thread->getSmartPointer();
        // tx_handler_thread->start();
    }

    /*void queue_tx_packet(const void* data, uint16_t len) {
        tx_lock.acquire();

        while (tx_packet_queue->writeAvailable() < len + 6) {
            tx_lock.release();

            printk("tx: too much\n");
            Proc::Thread::yield();

            tx_lock.acquire();
        }

        int dev_id = 0;

        tx_packet_queue->write(&dev_id, 4);
        tx_packet_queue->write(&len, 2);
        tx_packet_queue->write(data, len);

        tx_packet_event.raise();
        tx_lock.release();
    }

    void queue_tx_packet(Dev::NetDevice_SP net_dev, const void* data, uint16_t len) {
        tx_lock.acquire();

        while (tx_packet_queue->writeAvailable() < len + 6) {
            tx_lock.release();

            printk("tx: too much\n");
            Proc::Thread::yield();

            tx_lock.acquire();
        }

        tx_packet_queue->write(&net_dev->id, 4);
        tx_packet_queue->write(&len, 2);
        tx_packet_queue->write(data, len);

        tx_packet_event.raise();
        tx_lock.release();
    }

    // Make seperate threads for each interface pls
    void tx_packet_handler() {
        while (true) {
            tx_lock.acquire();

            if (tx_packet_queue->readAvailable() == 0) {
                tx_packet_event.wait();
                tx_lock.release();

                // printk("tx: too less\n");

                Proc::Thread::yield();
                continue;
            }

            int      dev_id;
            uint16_t len;
            uint8_t  buffer[2048];

            tx_packet_queue->read(&dev_id, 4);
            tx_packet_queue->read(&len, 2);
            tx_packet_queue->read(buffer, len);

            tx_lock.release();

            auto net_dev = Dev::get_net_device(dev_id);
            if (!net_dev)
                break;

            net_dev->send(buffer, len);
        }
    }

    Dev::NetDevice_SP get_interface_by_srcaddr(Net::mac_addr mac) {
        for (auto iterator = Dev::devices.getIterator(); auto device = iterator.next();) {
            if (device->type != Dev::DEV_NET)
                continue;

            auto net_dev = (Dev::NetDevice*) device;
            if (net_dev->info.ipv4.mac != mac)
                continue;

            return iterator.get_ptr();
        }

        return Dev::devices.get(-1);
    }

    Dev::NetDevice_SP get_interface_by_srcaddr(Net::ipv4_addr ipv4_addr) {
        for (auto iterator = Dev::devices.getIterator(); auto device = iterator.next();) {
            if (device->type != Dev::DEV_NET)
                continue;

            auto net_dev = (Dev::NetDevice*) device;
            if (net_dev->info.ipv4.addr != ipv4_addr)
                continue;

            return iterator.get_ptr();
        }

        return Dev::devices.get(-1);
    }

    Dev::NetDevice_SP get_interface_for_dst(Net::ipv4_addr ipv4_addr) {
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
    }*/
}
