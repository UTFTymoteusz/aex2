#include "protocol/udp.hpp"

#include "aex/net/socket.hpp"
#include "aex/printk.hpp"

#include "layer/ipv4.hpp"

using namespace AEX::Net;

namespace AEX::NetStack {
    // I need a R/W spinlock really bad

    Spinlock                        UDPProtocol::sockets_lock;
    Mem::Vector<UDPSocket*, 32, 32> UDPProtocol::sockets;

    optional<Socket*> UDPProtocol::createSocket(socket_type_t type) {
        if (type != socket_type_t::SOCK_DGRAM)
            return error_t::ESOCKTNOSUPPORT;

        auto socket = new UDPSocket();
        if (!socket)
            return error_t::ENOMEM;

        // Probably should only add this once bind()ing or something
        sockets_lock.acquire();
        sockets.pushBack(socket);
        sockets_lock.release();

        return socket;
    }

    void UDPProtocol::packetReceived(Net::ipv4_addr src, uint16_t src_port, Net::ipv4_addr dst,
                                     uint16_t dst_port, uint8_t* buffer, uint16_t len) {
        sockets_lock.acquire();

        for (int i = 0; i < sockets.count(); i++) {
            auto socket = sockets[i];
            if (!socket->active)
                continue;

            if (dst_port != socket->listen_port)
                continue;

            // Make this handle addresses like 192.168.0.255 properly pls
            if (dst != socket->listen_address && socket->listen_address != IPv4Layer::ANY)
                continue;

            socket->packetReceived(src, src_port, buffer, len);
        }

        sockets_lock.release();
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
    }

    error_t UDPSocket::bind(const sockaddr* addr) {
        sockaddr_inet* _addr = (sockaddr_inet*) addr;
        if (_addr->domain != socket_domain_t::AF_INET || this->active)
            return error_t::EINVAL;

        this->listen_address = _addr->addr;
        this->listen_port    = _addr->port;
        this->active         = true;

        return error_t::ENONE;
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