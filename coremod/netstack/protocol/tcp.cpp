#include "protocol/tcp.hpp"

#include "aex/net.hpp"
#include "aex/printk.hpp"

#include "checksum.hpp"
#include "layer/ipv4.hpp"
#include "layer/udp.hpp"
#include "socket/tcp.hpp"

using namespace AEX::Net;

namespace AEX::NetStack {
    // I need a R/W spinlock really bad

    Spinlock                        TCPProtocol::sockets_lock;
    Mem::Vector<TCPSocket*, 32, 32> TCPProtocol::sockets;

    Spinlock  TCPProtocol::_ports_lock;
    uint32_t* TCPProtocol::_port_bitmap       = nullptr;
    uint16_t  TCPProtocol::_port_dynamic_last = 49151;

    Mem::SmartPointer<Proc::Thread> TCPProtocol::_loop_thread;

    void TCPProtocol::init() {
        _port_bitmap = new uint32_t[65536 / sizeof(uint32_t) / 8];

        auto thread = new Proc::Thread(nullptr, (void*) loop, 16384, nullptr);

        _loop_thread = thread->getSmartPointer();
        _loop_thread->start();
    }

    void TCPProtocol::loop() {
        while (true) {
            sockets_lock.acquire();

            for (int i = 0; i < sockets.count(); i++)
                sockets[i]->tick();

            sockets_lock.release();
            Proc::Thread::sleep(100);
        }
    }

    optional<Socket*> TCPProtocol::createSocket(socket_type_t type) {
        if (type != socket_type_t::SOCK_STREAM)
            return ESOCKTNOSUPPORT;

        auto socket = new TCPSocket();
        if (!socket)
            return ENOMEM;

        uint16_t port = allocateDynamicPort();
        if (port == 0) {
            delete socket;
            // fix this pls
            return ENOMEM;
        }

        socket->source_port = port;

        sockets_lock.acquire();
        sockets.pushBack(socket);
        sockets_lock.release();

        printk("new tcp socket with source port of %i\n", socket->source_port);

        return socket;
    }

    void TCPProtocol::packetReceived(Net::ipv4_addr src, uint16_t src_port, Net::ipv4_addr dst,
                                     uint16_t dst_port, uint8_t* buffer, uint16_t len) {
        if (dst_port == 0)
            return;

        sockets_lock.acquire();

        for (int i = 0; i < sockets.count(); i++) {
            auto socket = sockets[i];
            if (dst_port != socket->source_port)
                continue;

            if (src != socket->destination_address)
                continue;

            socket->packetReceived(src, src_port, buffer, len);
        }

        sockets_lock.release();
    }


    uint16_t TCPProtocol::allocateDynamicPort() {
        _port_dynamic_last++;
        if (_port_dynamic_last == 0)
            _port_dynamic_last = 49152;

        uint32_t ii = _port_dynamic_last / (sizeof(uint32_t) * 8);
        uint16_t ib = _port_dynamic_last % (sizeof(uint32_t) * 8);

        uint32_t buffer;

        _ports_lock.acquire();

        for (int i = 49152; i <= 65536; i++) {
            buffer = _port_bitmap[ii];

            for (; ib < sizeof(uint32_t) * 8; ib++) {
                if (buffer & (1 << ib))
                    continue;

                _port_bitmap[ii] |= 1 << ib;

                uint16_t port      = ii * 32 + ib;
                _port_dynamic_last = port;

                _ports_lock.release();

                return port;
            }

            ib = 0;
            ii++;

            if (ii >= 8192)
                ii = 49152 / sizeof(uint32_t);
        }

        _ports_lock.release();

        return 0;
    }

    bool TCPProtocol::allocatePort(uint16_t port) {
        uint32_t ii = port / (sizeof(uint32_t) * 8);
        uint16_t ib = port % (sizeof(uint32_t) * 8);

        auto scopeLock = ScopeSpinlock(_ports_lock);

        if (_port_bitmap[ii] & (1 << ib))
            return false;

        _port_bitmap[ii] |= 1 << ib;
        return true;
    }

    void TCPProtocol::freePort(uint16_t port) {
        uint32_t ii = port / (sizeof(uint32_t) * 8);
        uint16_t ib = port % (sizeof(uint32_t) * 8);

        auto scopeLock = ScopeSpinlock(_ports_lock);

        _port_bitmap[ii] &= ~(1 << ib);
    }
}