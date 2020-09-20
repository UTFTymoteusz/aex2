#include "udp.hpp"

#include "aex/net.hpp"
#include "aex/printk.hpp"

#include "checksum.hpp"
#include "layer/network/ipv4.hpp"
#include "layer/transport/udp.hpp"
#include "socket/inet/udp.hpp"

using namespace AEX;
using namespace AEX::Net;

namespace NetStack {
    // I need a R/W spinlock really bad

    Spinlock                        UDPProtocol::sockets_lock;
    Mem::Vector<UDPSocket*, 32, 32> UDPProtocol::sockets;

    Spinlock  UDPProtocol::m_ports_lock;
    uint32_t* UDPProtocol::m_port_bitmap       = nullptr;
    uint16_t  UDPProtocol::m_port_dynamic_last = 49151;

    void UDPProtocol::init() {
        m_port_bitmap = new uint32_t[65536 / sizeof(uint32_t) / 8];
    }

    optional<Socket_SP> UDPProtocol::createSocket(socket_type_t type) {
        if (type != socket_type_t::SOCK_DGRAM)
            return ESOCKTNOSUPPORT;

        auto socket = new UDPSocket();
        if (!socket)
            return ENOMEM;

        uint16_t port = allocateDynamicPort();
        if (port == 0) {
            delete socket;
            // fix this pls
            return ENOMEM;
        }

        socket->source_port = port;

        // Probably should only add this once bind()ing or something
        sockets_lock.acquire();
        sockets.push(socket);
        sockets_lock.release();

        printk("new udp socket with source port of %i\n", socket->source_port);

        return Net::Socket_SP(socket);
    }

    void UDPProtocol::packetReceived(Net::ipv4_addr src, uint16_t src_port, Net::ipv4_addr dst,
                                     uint16_t dst_port, const uint8_t* buffer, uint16_t len) {
        if (dst_port == 0)
            return;

        sockets_lock.acquire();

        for (int i = 0; i < sockets.count(); i++) {
            auto socket = sockets[i];
            if (dst_port != socket->source_port)
                continue;

            // Make this handle addresses like 192.168.0.255 properly pls
            if (dst != socket->source_address && socket->source_address != IPv4_ANY)
                continue;

            socket->packetReceived(src, src_port, buffer, len);
        }

        sockets_lock.release();
    }


    uint16_t UDPProtocol::allocateDynamicPort() {
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

    bool UDPProtocol::allocatePort(uint16_t port) {
        uint32_t ii = port / (sizeof(uint32_t) * 8);
        uint16_t ib = port % (sizeof(uint32_t) * 8);

        ScopeSpinlock scopeLock(m_ports_lock);

        if (m_port_bitmap[ii] & (1 << ib))
            return false;

        m_port_bitmap[ii] |= 1 << ib;
        return true;
    }

    void UDPProtocol::freePort(uint16_t port) {
        uint32_t ii = port / (sizeof(uint32_t) * 8);
        uint16_t ib = port % (sizeof(uint32_t) * 8);

        ScopeSpinlock scopeLock(m_ports_lock);

        m_port_bitmap[ii] &= ~(1 << ib);
    }
}