#include "tcp.hpp"

#include "aex/net.hpp"
#include "aex/printk.hpp"

#include "checksum.hpp"
#include "layer/network/ipv4.hpp"
#include "socket/inet/tcp.hpp"

using namespace AEX;
using namespace AEX::Net;

namespace NetStack {
    // Also, i could have the main loop bong check an optional<Promise>, and check if its fulfilled
    // to avoid wanks with ARP
    // Or better yet, [b]oroutines
    // Have ARP with callbacks and have the callback set a flag that'll make loop enter the logic

    RWSpinlock                      TCPProtocol::sockets_lock;
    Mem::Vector<TCPSocket*, 32, 32> TCPProtocol::sockets;

    Spinlock  TCPProtocol::m_ports_lock;
    uint32_t* TCPProtocol::m_port_bitmap       = nullptr;
    uint16_t  TCPProtocol::m_port_dynamic_last = 49151;

    Proc::Thread* TCPProtocol::m_loop_thread;

    void TCPProtocol::init() {
        m_port_bitmap = new uint32_t[65536 / sizeof(uint32_t) / 8];

        auto thread = Proc::Thread::create(1, (void*) loop, 16384, nullptr);

        m_loop_thread = thread.value;
        m_loop_thread->start();
        m_loop_thread->detach();
    }

    void TCPProtocol::loop() {
        while (true) {
            sockets_lock.acquire_read();

            for (int i = 0; i < sockets.count(); i++)
                sockets[i]->tick();

            sockets_lock.release_read();
            Proc::Thread::sleep(100);
        }
    }

    optional<Socket_SP> TCPProtocol::createSocket(socket_type_t type) {
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

        sockets_lock.acquire_write();
        sockets.push(socket);
        sockets_lock.release_write();

        printk("new tcp socket with source port of %i\n", socket->source_port);

        return *socket->thisSmartPtr;
    }

    void TCPProtocol::packetReceived(Net::ipv4_addr src, uint16_t src_port, Net::ipv4_addr dst,
                                     uint16_t dst_port, const uint8_t* buffer, uint16_t len) {
        if (dst_port == 0)
            return;

        sockets_lock.acquire_read();

        for (int i = 0; i < sockets.count(); i++) {
            auto socket = sockets[i];
            if (dst_port != socket->source_port || src_port != socket->destination_port ||
                socket->m_block.state == TCP_LISTEN || socket->m_block.state == TCP_CLOSED)
                continue;

            socket->packetReceived(src, src_port, buffer, len);
            sockets_lock.release_read();

            return;
        }

        for (int i = 0; i < sockets.count(); i++) {
            auto socket = sockets[i];
            if (dst_port != socket->source_port || socket->m_block.state != TCP_LISTEN)
                continue;

            socket->packetReceived(src, src_port, buffer, len);
            sockets_lock.release_read();

            return;
        }

        sockets_lock.release_read();

        // send rst pls
    }

    uint16_t TCPProtocol::allocateDynamicPort() {
        m_ports_lock.acquire();

        m_port_dynamic_last++;
        if (m_port_dynamic_last == 0)
            m_port_dynamic_last = 49152;

        uint32_t ii = m_port_dynamic_last / (sizeof(uint32_t) * 8);
        uint16_t ib = m_port_dynamic_last % (sizeof(uint32_t) * 8);

        uint32_t buffer;

        for (int i = 49152; i <= 65536; i++) {
            buffer = m_port_bitmap[ii];

            for (; ib < sizeof(uint32_t) * 8; ib++) {
                if (buffer & (1 << ib))
                    continue;

                m_port_bitmap[ii] |= 1 << ib;

                uint16_t port       = ii * 32 + ib;
                m_port_dynamic_last = port;

                m_ports_lock.release();

                return port;
            }

            ib = 0;
            ii++;

            if (ii >= 8192)
                ii = 49152 / sizeof(uint32_t);
        }

        m_ports_lock.release();

        return 0;
    }

    bool TCPProtocol::allocatePort(uint16_t port) {
        uint32_t ii = port / (sizeof(uint32_t) * 8);
        uint16_t ib = port % (sizeof(uint32_t) * 8);

        ScopeSpinlock scopeLock(m_ports_lock);

        if (m_port_bitmap[ii] & (1 << ib))
            return false;

        m_port_bitmap[ii] |= 1 << ib;
        return true;
    }

    void TCPProtocol::freePort(uint16_t port) {
        uint32_t ii = port / (sizeof(uint32_t) * 8);
        uint16_t ib = port % (sizeof(uint32_t) * 8);

        ScopeSpinlock scopeLock(m_ports_lock);

        m_port_bitmap[ii] &= ~(1 << ib);
    }

    void TCPProtocol::pushSocket(TCPSocket* socket) {
        sockets_lock.acquire_write();
        sockets.push(socket);
        sockets_lock.release_write();
    }
}