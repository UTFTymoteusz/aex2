#include "protocol/udp.hpp"

#include "aex/net/socket.hpp"
#include "aex/printk.hpp"

#include "checksum.hpp"
#include "layer/ipv4.hpp"
#include "layer/udp.hpp"
#include "tx_core.hpp"

using namespace AEX::Net;

namespace AEX::NetStack {
    // I need a R/W spinlock really bad

    Spinlock                        UDPProtocol::sockets_lock;
    Mem::Vector<UDPSocket*, 32, 32> UDPProtocol::sockets;

    Spinlock  UDPProtocol::_ports_lock;
    uint32_t* UDPProtocol::_port_bitmap       = nullptr;
    uint16_t  UDPProtocol::_port_dynamic_last = 49151;

    void UDPProtocol::init() {
        _port_bitmap = new uint32_t[65536 / sizeof(uint32_t) / 8];
    }

    optional<Socket*> UDPProtocol::createSocket(socket_type_t type) {
        if (type != socket_type_t::SOCK_DGRAM)
            return error_t::ESOCKTNOSUPPORT;

        auto socket = new UDPSocket();
        if (!socket)
            return error_t::ENOMEM;

        uint16_t port = allocateDynamicPort();
        if (port == 0) {
            delete socket;
            // fix this pls
            return error_t::ENOMEM;
        }

        socket->source_port = port;

        // Probably should only add this once bind()ing or something
        sockets_lock.acquire();
        sockets.pushBack(socket);
        sockets_lock.release();

        printk("new udp socket with source port of %i\n", socket->source_port);

        return socket;
    }

    void UDPProtocol::packetReceived(Net::ipv4_addr src, uint16_t src_port, Net::ipv4_addr dst,
                                     uint16_t dst_port, uint8_t* buffer, uint16_t len) {
        sockets_lock.acquire();

        for (int i = 0; i < sockets.count(); i++) {
            auto socket = sockets[i];
            if (dst_port != socket->source_port)
                continue;

            // Make this handle addresses like 192.168.0.255 properly pls
            if (dst != socket->source_address && socket->source_address != IPv4Layer::ANY)
                continue;

            socket->packetReceived(src, src_port, buffer, len);
        }

        sockets_lock.release();
    }


    uint16_t UDPProtocol::allocateDynamicPort() {
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

    bool UDPProtocol::allocatePort(uint16_t port) {
        uint32_t ii = port / (sizeof(uint32_t) * 8);
        uint16_t ib = port % (sizeof(uint32_t) * 8);

        auto scopeLock = ScopeSpinlock(_ports_lock);

        if (_port_bitmap[ii] & (1 << ib))
            return false;

        _port_bitmap[ii] |= 1 << ib;
        return true;
    }

    void UDPProtocol::freePort(uint16_t port) {
        uint32_t ii = _port_dynamic_last / (sizeof(uint32_t) * 8);
        uint16_t ib = _port_dynamic_last % (sizeof(uint32_t) * 8);

        auto scopeLock = ScopeSpinlock(_ports_lock);

        _port_bitmap[ii] &= ~(1 << ib);
    }

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
    }

    error_t UDPSocket::bind(const sockaddr* addr) {
        sockaddr_inet* _addr = (sockaddr_inet*) addr;
        if (_addr->domain != socket_domain_t::AF_INET)
            return error_t::EINVAL;

        if (!UDPProtocol::allocatePort(_addr->port))
            return error_t::EADDRINUSE;

        if (this->source_port != 0)
            UDPProtocol::freePort(this->source_port);

        this->source_address = _addr->addr;
        this->source_port    = _addr->port;

        printk("udp: Bound a socket to %i\n", _addr->port);

        return error_t::ENONE;
    }

    optional<size_t> UDPSocket::sendto(const void* buffer, size_t len, int flags,
                                       const sockaddr* dst_addr) {
        if (!buffer || !dst_addr)
            return error_t::EINVAL;

        auto _dst_addr = (sockaddr_inet*) dst_addr;
        auto net_dev   = get_interface_for_dst(_dst_addr->addr);
        if (!net_dev.isValid())
            return error_t::ENETUNREACH;

        auto en_buffer =
            IPv4Layer::encapsulate(net_dev->ipv4_addr, _dst_addr->addr, ipv4_protocol_t::IPv4_UDP,
                                   net_dev, len + sizeof(udp_header));
        if (!en_buffer.has_value)
            return en_buffer.error_code;

        auto _udp_header = (udp_header*) en_buffer.value->alloc(sizeof(udp_header));

        _udp_header->source_port      = this->source_port;
        _udp_header->destination_port = _dst_addr->port;
        _udp_header->total_length     = len + sizeof(udp_header);
        _udp_header->checksum         = 0x0000;

        auto _fake_header = udp_fake_ipv4_header();

        _fake_header.source      = net_dev->ipv4_addr;
        _fake_header.destination = _dst_addr->addr;
        _fake_header.zero        = 0x00;
        _fake_header.protocol    = ipv4_protocol_t::IPv4_UDP;
        _fake_header.length      = len + sizeof(udp_header);

        uint32_t sum = sum_bytes(&_fake_header, sizeof(udp_fake_ipv4_header)) +
                       sum_bytes(_udp_header, sizeof(udp_header)) + sum_bytes(buffer, len);
        _udp_header->checksum = to_checksum(sum);

        en_buffer.value->write(buffer, len);
        net_dev->send(en_buffer.value->get(), en_buffer.value->length());

        en_buffer.value->release();

        return len;
    }

    optional<size_t> UDPSocket::recvfrom(void* buffer, size_t len, int flags, sockaddr* src_addr) {
        _lock.acquire();

        // Make nonblocking flag pls
        while (!_first_datagram) {
            _lock.release();
            _event.wait();
            _lock.acquire();
        }

        auto dgram = _first_datagram;

        _first_datagram = _first_datagram->next;
        if (!_first_datagram)
            _last_datagram = nullptr;

        _lock.release();

        size_t dgram_len = dgram->len;
        memcpy(buffer, dgram->buffer, min<size_t>(dgram->len, len));

        if (src_addr) {
            auto _src_addr = (sockaddr_inet*) src_addr;

            _src_addr->domain = socket_domain_t::AF_INET;
            _src_addr->addr   = dgram->source_address;
            _src_addr->port   = dgram->source_port;
        }

        Heap::free(dgram);

        return dgram_len;
    }

    void UDPSocket::packetReceived(Net::ipv4_addr src, uint16_t src_port, uint8_t* buffer,
                                   uint16_t len) {
        auto dgram = (datagram*) Heap::malloc(sizeof(datagram) + len);

        dgram->source_address = src;
        dgram->source_port    = src_port;
        dgram->len            = len;

        memcpy(dgram->buffer, buffer, len);

        _lock.acquire();

        if (_last_datagram)
            _last_datagram->next = dgram;
        else
            _first_datagram = dgram;

        _last_datagram = dgram;

        _event.raise();
        _lock.release();
    }

}