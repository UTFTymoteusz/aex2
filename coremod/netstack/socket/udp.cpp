#include "socket/udp.hpp"

#include "aex/ipc/event.hpp"
#include "aex/net.hpp"
#include "aex/printk.hpp"
#include "aex/spinlock.hpp"

#include "checksum.hpp"
#include "layer/ipv4.hpp"
#include "layer/udp.hpp"
#include "protocol/udp.hpp"
#include "tx_core.hpp"

#include <stddef.h>
#include <stdint.h>

using namespace AEX::Mem;
using namespace AEX::Net;

namespace AEX::NetStack {
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

        while (_first_datagram) {
            auto next = _first_datagram->next;

            Heap::free(_first_datagram);

            _first_datagram = next;
        }
    }

    error_t UDPSocket::connect(const sockaddr* addr) {
        sockaddr_inet* _addr = (sockaddr_inet*) addr;
        if (_addr->domain != socket_domain_t::AF_INET)
            return EINVAL;

        destination_address = _addr->addr;
        destination_port    = _addr->port;

        printk("udp: Connected a socket to port %i\n", _addr->port);

        return ENONE;
    }


    error_t UDPSocket::bind(const sockaddr* addr) {
        sockaddr_inet* _addr = (sockaddr_inet*) addr;
        if (_addr->domain != socket_domain_t::AF_INET)
            return EINVAL;

        if (!UDPProtocol::allocatePort(_addr->port))
            return EADDRINUSE;

        if (this->source_port != 0)
            UDPProtocol::freePort(this->source_port);

        this->source_address = _addr->addr;
        this->source_port    = _addr->port;

        printk("udp: Bound a socket to port %i\n", _addr->port);

        return ENONE;
    }

    optional<size_t> UDPSocket::sendTo(const void* buffer, size_t len, int flags,
                                       const sockaddr* dst_addr) {
        if (!buffer)
            return EINVAL;

        Net::ipv4_addr addr;
        uint16_t       port;

        if (dst_addr) {
            auto _dst_addr = (sockaddr_inet*) dst_addr;

            addr = _dst_addr->addr;
            port = _dst_addr->port;
        }
        else {
            if (destination_port == 0)
                return ENOTCONN;

            addr = destination_address;
            port = destination_port;
        }

        auto net_dev = get_interface_for_dst(addr);
        if (!net_dev.isValid())
            return ENETUNREACH;

        auto en_buffer =
            IPv4Layer::encapsulate(net_dev->info.ipv4.addr, addr, ipv4_protocol_t::IPv4_UDP,
                                   net_dev, len + sizeof(udp_header));
        if (!en_buffer.has_value)
            return en_buffer.error_code;

        auto _udp_header = (udp_header*) en_buffer.value->alloc(sizeof(udp_header));

        _udp_header->source_port      = this->source_port;
        _udp_header->destination_port = port;
        _udp_header->total_length     = len + sizeof(udp_header);
        _udp_header->checksum         = 0x0000;

        auto _fake_header = udp_fake_ipv4_header();

        _fake_header.source      = net_dev->info.ipv4.addr;
        _fake_header.destination = destination_address;
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

    optional<size_t> UDPSocket::receiveFrom(void* buffer, size_t len, int flags,
                                            sockaddr* src_addr) {
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
        if (_buffered_size + len > UDP_SOCKET_BUFFER_SIZE) {
            printk("udp: Socket at port %i buffer overflow\n", source_port);
            return;
        }

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