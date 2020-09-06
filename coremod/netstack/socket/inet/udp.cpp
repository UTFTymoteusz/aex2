#include "socket/inet/udp.hpp"

#include "aex/ipc/event.hpp"
#include "aex/net.hpp"
#include "aex/printk.hpp"
#include "aex/spinlock.hpp"

#include "checksum.hpp"
#include "layer/network/ipv4.hpp"
#include "layer/transport/udp.hpp"
#include "protocol/inet/udp.hpp"

#include <stddef.h>
#include <stdint.h>

using namespace AEX;
using namespace AEX::Mem;
using namespace AEX::Net;

namespace NetStack {
    UDPSocket::~UDPSocket() {
        UDPProtocol::sockets_lock.acquire();

        for (int i = 0; i < UDPProtocol::sockets.count(); i++) {
            if (UDPProtocol::sockets[i] != this)
                continue;

            UDPProtocol::sockets.erase(i);
            break;
        }

        UDPProtocol::sockets_lock.release();

        if (source_port != 0)
            UDPProtocol::freePort(source_port);

        while (m_first_datagram) {
            auto next = m_first_datagram->next;

            Heap::free(m_first_datagram);

            m_first_datagram = next;
        }
    }

    error_t UDPSocket::connect(const sockaddr* addr) {
        sockaddr_inet* m_addr = (sockaddr_inet*) addr;
        if (m_addr->domain != socket_domain_t::AF_INET)
            return EINVAL;

        destination_address = m_addr->addr;
        destination_port    = m_addr->port;

        printk("udp: Connected a socket to port %i\n", m_addr->port);

        return ENONE;
    }


    error_t UDPSocket::bind(const sockaddr* addr) {
        sockaddr_inet* m_addr = (sockaddr_inet*) addr;
        if (m_addr->domain != socket_domain_t::AF_INET)
            return EINVAL;

        if (!UDPProtocol::allocatePort(m_addr->port))
            return EADDRINUSE;

        if (this->source_port != 0)
            UDPProtocol::freePort(this->source_port);

        this->source_address = m_addr->addr;
        this->source_port    = m_addr->port;

        printk("udp: Bound a socket to port %i\n", m_addr->port);

        return ENONE;
    }

    optional<size_t> UDPSocket::sendTo(const void* buffer, size_t len, int flags,
                                       const sockaddr* dst_addr) {
        if (!buffer)
            return EINVAL;

        Net::ipv4_addr addr;
        uint16_t       port;

        if (dst_addr) {
            auto m_dst_addr = (sockaddr_inet*) dst_addr;

            addr = m_dst_addr->addr;
            port = m_dst_addr->port;
        }
        else {
            if (destination_port == 0)
                return ENOTCONN;

            addr = destination_address;
            port = destination_port;
        }

        auto dev = IPv4Layer::get_interface(addr);
        if (!dev)
            return ENETUNREACH;

        uint8_t  packet[IPv4Layer::encaps_len(sizeof(udp_header) + len)];
        uint8_t* payload = IPv4Layer::encapsulate(packet, sizeof(udp_header) + len, addr, IPv4_UDP);

        auto m_udp_header = (udp_header*) payload;

        m_udp_header->source_port      = this->source_port;
        m_udp_header->destination_port = port;
        m_udp_header->total_length     = len + sizeof(udp_header);
        m_udp_header->checksum         = 0x0000;

        auto m_fake_header = udp_fake_ipv4_header();

        m_fake_header.source      = dev->info.ipv4.addr;
        m_fake_header.destination = destination_address;
        m_fake_header.zero        = 0x00;
        m_fake_header.protocol    = IPv4_UDP;
        m_fake_header.length      = len + sizeof(udp_header);

        uint32_t sum = sum_bytes(&m_fake_header, sizeof(udp_fake_ipv4_header)) +
                       sum_bytes(m_udp_header, sizeof(udp_header)) + sum_bytes(buffer, len);
        m_udp_header->checksum = to_checksum(sum);

        memcpy(payload + sizeof(udp_header), buffer, len);

        IPv4Layer::route(packet, IPv4Layer::encaps_len(sizeof(udp_header) + len));
        return len;
    }

    optional<size_t> UDPSocket::receiveFrom(void* buffer, size_t len, int flags,
                                            sockaddr* src_addr) {
        m_lock.acquire();

        // Make nonblocking flag pls
        while (!m_first_datagram) {
            m_event.wait();
            m_lock.release();

            Proc::Thread::yield();

            m_lock.acquire();
        }

        auto dgram = m_first_datagram;

        m_first_datagram = m_first_datagram->next;
        if (!m_first_datagram)
            m_last_datagram = nullptr;

        m_lock.release();

        size_t dgram_len = dgram->len;
        memcpy(buffer, dgram->buffer, min<size_t>(dgram->len, len));

        if (src_addr) {
            auto m_src_addr = (sockaddr_inet*) src_addr;

            m_src_addr->domain = socket_domain_t::AF_INET;
            m_src_addr->addr   = dgram->source_address;
            m_src_addr->port   = dgram->source_port;
        }

        Heap::free(dgram);

        return dgram_len;
    }

    void UDPSocket::packetReceived(Net::ipv4_addr src, uint16_t src_port, const uint8_t* buffer,
                                   uint16_t len) {
        if (m_buffered_size + len > UDP_SOCKET_BUFFER_SIZE) {
            printk("udp: Socket at port %i buffer overflow\n", source_port);
            return;
        }

        auto dgram = (datagram*) Heap::malloc(sizeof(datagram) + len);

        dgram->source_address = src;
        dgram->source_port    = src_port;
        dgram->len            = len;

        memcpy(dgram->buffer, buffer, len);

        m_lock.acquire();

        if (m_last_datagram)
            m_last_datagram->next = dgram;
        else
            m_first_datagram = dgram;

        m_last_datagram = dgram;

        m_event.raise();
        m_lock.release();
    }
}