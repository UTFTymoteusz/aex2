#include "socket/tcp.hpp"

#include "aex/ipc/event.hpp"
#include "aex/mem.hpp"
#include "aex/net.hpp"
#include "aex/printk.hpp"
#include "aex/spinlock.hpp"
#include "aex/sys/time.hpp"

#include "checksum.hpp"
#include "layer/ipv4.hpp"
#include "layer/tcp.hpp"
#include "protocol/tcp.hpp"
#include "tx_core.hpp"

#include <stddef.h>
#include <stdint.h>

using namespace AEX::Mem;
using namespace AEX::Net;

namespace AEX::NetStack {
    TCPSocket::~TCPSocket() {
        TCPProtocol::sockets_lock.acquire();

        for (int i = 0; i < TCPProtocol::sockets.count(); i++) {
            if (TCPProtocol::sockets[i] != this)
                continue;

            TCPProtocol::sockets.erase(i);
            break;
        }

        TCPProtocol::sockets_lock.release();

        if (source_port != 0)
            TCPProtocol::freePort(source_port);
    }

    error_t TCPSocket::connect(const sockaddr* addr) {
        sockaddr_inet* _addr = (sockaddr_inet*) addr;
        if (_addr->domain != socket_domain_t::AF_INET)
            return error_t::EINVAL;

        _lock.acquire();

        if (_state != tcp_state_t::TCP_CLOSED) {
            _lock.release();
            return error_t::EISCONN;
        }

        this->destination_address = _addr->addr;
        this->destination_port    = _addr->port;

        auto seg = new (Heap::malloc(sizeof(segment) + 8)) segment();

        seg->seq           = _current_seq;
        seg->expected_ack  = _current_seq + 1;
        seg->len           = 8;
        seg->data_offset   = 28;
        seg->flags         = TCP_SYN;
        seg->retransmit_at = Sys::get_uptime() + TCP_RETRANSMIT_INTERVAL;

        seg->buffer[0] = 2;
        seg->buffer[1] = 4;
        seg->buffer[2] = 0x05;
        seg->buffer[3] = 0x78;
        seg->buffer[4] = 0x01;
        seg->buffer[5] = 0x01;
        seg->buffer[6] = 0x01;
        seg->buffer[7] = 0x00;

        _current_seq++;

        pushSegment(seg);

        _state = tcp_state_t::TCP_SYN_SENT;
        _lock.release();

        sendSegment(seg);

        while (true) {
            _lock.acquire();

            if (_state == tcp_state_t::TCP_ESTABLISHED)
                break;

            if (_state == tcp_state_t::TCP_CLOSED) {
                _lock.release();
                return error_t::ECONNREFUSED;
            }

            _rx_event.wait();
            _lock.release();

            Proc::Thread::yield();
        }

        _lock.release();

        return error_t::ENONE;
    }


    error_t TCPSocket::bind(const sockaddr* addr) {
        sockaddr_inet* _addr = (sockaddr_inet*) addr;
        if (_addr->domain != socket_domain_t::AF_INET)
            return error_t::EINVAL;

        if (!TCPProtocol::allocatePort(_addr->port))
            return error_t::EADDRINUSE;

        if (this->source_port != 0)
            TCPProtocol::freePort(this->source_port);

        this->source_address = _addr->addr;
        this->source_port    = _addr->port;

        printk("tcp: Bound a socket to port %i\n", _addr->port);

        return error_t::ENONE;
    }

    optional<size_t> TCPSocket::sendTo(const void* buffer, size_t len, int flags,
                                       const sockaddr* dst_addr) {
        if (!buffer)
            return error_t::EINVAL;

        if (destination_port == 0)
            return error_t::ENOTCONN;

        if (dst_addr)
            return error_t::EISCONN;

        _lock.acquire();

        while (_tx_circ_buffer.writeAvailable() < len) {
            _tx_event.wait();
            _lock.release();

            Proc::Thread::yield();

            _lock.acquire();
        }

        _tx_circ_buffer.write(buffer, len);
        _lock.release();

        return len;
    }

    optional<size_t> TCPSocket::receiveFrom(void* buffer, size_t len, int flags,
                                            sockaddr* src_addr) {
        if (src_addr)
            return error_t::EISCONN;

        _lock.acquire();

        int read_len = min<size_t>(_rx_circ_buffer.readAvailable(), len);

        while (_rx_circ_buffer.readAvailable() == 0) {
            _rx_event.wait();
            _lock.release();

            Proc::Thread::yield();

            _lock.acquire();
            read_len = min<size_t>(_rx_circ_buffer.readAvailable(), len);
        }

        _rx_circ_buffer.read(buffer, read_len);
        _lock.release();

        return read_len;
    }

    void TCPSocket::tick() {
        _lock.acquire();

        auto     _segment = _first_segment;
        uint64_t uptime;

        while (_segment) {
            uptime = Sys::get_uptime();

            if (_segment->retransmit_at < uptime) {
                _segment->retransmit_at = uptime + TCP_RETRANSMIT_INTERVAL;
                _segment->retries++;

                if (_segment->retries > 5) {
                    sendRST();

                    destination_port = 0;
                    _state           = tcp_state_t::TCP_CLOSED;
                }

                int seg_len = _segment->len;

                _lock.release();

                // I need some deletion prevention for the segments
                sendSegment(_segment);

                _lock.acquire();
                _current_seq += seg_len;
            }

            _segment = _segment->next;
        }

        uint8_t buffer[1400];

        int mss       = _remote_mss;
        int available = _tx_circ_buffer.readAvailable();

        while (available > 0) {
            int len = min<int>(available, mss);

            _tx_circ_buffer.read(buffer, len);
            _lock.release();

            auto seg = new (Heap::malloc(sizeof(segment) + len)) segment();

            seg->seq           = _current_seq;
            seg->expected_ack  = _current_seq + len;
            seg->len           = len;
            seg->data_offset   = 20;
            seg->flags         = (tcp_flags_t)(tcp_flags_t::TCP_PSH | tcp_flags_t::TCP_ACK);
            seg->retransmit_at = Sys::get_uptime() + TCP_RETRANSMIT_INTERVAL;

            memcpy(seg->buffer, buffer, len);

            sendSegment(seg, _remote_seq);

            _lock.acquire();

            _current_seq += len;
            available = _tx_circ_buffer.readAvailable();
        }

        _lock.release();
    }

    void TCPSocket::sendSegment(segment* segment, int ack) {
        auto net_dev = get_interface_for_dst(destination_address);
        if (!net_dev.isValid())
            kpanic("tcp popped");

        auto en_buffer = IPv4Layer::encapsulate(net_dev->info.ipv4.addr, destination_address,
                                                ipv4_protocol_t::IPv4_TCP, net_dev,
                                                segment->len + sizeof(tcp_header));
        if (!en_buffer.has_value)
            kpanic("tcp popped 2 (%s)", strerror(en_buffer.error_code));

        auto _tcp_header = (tcp_header*) en_buffer.value->alloc(sizeof(tcp_header));

        _tcp_header->source_port      = this->source_port;
        _tcp_header->destination_port = this->destination_port;
        _tcp_header->checksum         = 0x0000;
        _tcp_header->seq_number       = segment->seq;
        _tcp_header->ack_number       = ack;
        _tcp_header->window           = 8192;
        _tcp_header->checksum         = 0x0000;
        _tcp_header->urgent_pointer   = 0;

        _tcp_header->fucking_bitvalues = (segment->data_offset << 10) | segment->flags;

        auto _fake_header = tcp_fake_ipv4_header();

        _fake_header.source      = net_dev->info.ipv4.addr;
        _fake_header.destination = destination_address;
        _fake_header.zero        = 0x00;
        _fake_header.protocol    = ipv4_protocol_t::IPv4_TCP;
        _fake_header.length      = segment->len + sizeof(tcp_header);

        uint32_t sum = sum_bytes(&_fake_header, sizeof(tcp_fake_ipv4_header)) +
                       sum_bytes(_tcp_header, sizeof(tcp_header)) +
                       sum_bytes(segment->buffer, segment->len);
        _tcp_header->checksum = to_checksum(sum);

        en_buffer.value->write(segment->buffer, segment->len);
        net_dev->send(en_buffer.value->get(), en_buffer.value->length());

        en_buffer.value->release();
    }

    void TCPSocket::pushSegment(segment* segment) {
        if (!_last_segment) {
            _first_segment = segment;
            _last_segment  = segment;

            return;
        }

        _last_segment->next = segment;
        _last_segment       = segment;
    }

    void TCPSocket::sendACK(int to_ack) {
        printk("to ack: %i\n", to_ack);

        auto seg = segment();

        seg.seq          = _current_seq;
        seg.expected_ack = 0;
        seg.len          = 0;
        seg.data_offset  = 20;
        seg.flags        = TCP_ACK;

        sendSegment(&seg, to_ack);
    }


    void TCPSocket::sendRST() {
        /*auto seg = segment();

        seg.seq         = _current_seq;
        seg.ack         = _current_seq + 1;
        seg.len         = 8;
        seg.data_offset = 28;
        seg.flags       = TCP_SYN;*/

        kpanic("rst wooo");
    }

    void TCPSocket::packetReceived(Net::ipv4_addr src, uint16_t src_port, uint8_t* buffer,
                                   uint16_t len) {
        if (_state == tcp_state_t::TCP_CLOSED)
            return;

        _lock.acquire();

        int data_len = 0;

        auto _tcp_header = (tcp_header*) buffer;

        uint16_t hdr_len = (uint16_t) _tcp_header->fucking_bitvalues >> 10;
        uint16_t flags   = (uint16_t) _tcp_header->fucking_bitvalues;

        if (flags & tcp_flags_t::TCP_ACK) {
            printk("got an ack\n");

            auto segment = _first_segment;

            while (segment) {
                if (segment->expected_ack != _tcp_header->ack_number) {
                    segment = segment->next;
                    continue;
                }

                if (!segment->prev) {
                    _first_segment = segment->next;

                    if (_last_segment == segment)
                        _last_segment = nullptr;

                    delete segment;
                    break;
                }

                segment->prev->next = segment->next;
                if (_last_segment == segment)
                    _last_segment = segment->prev;

                delete segment;
            }

            _state_state = 1;
        }

        if (flags & tcp_flags_t::TCP_PSH) {
            int psh_data_len = len - hdr_len;

            if (_rx_circ_buffer.writeAvailable() < data_len) {
                sendRST();
                return;
            }

            _rx_circ_buffer.write(buffer + hdr_len, psh_data_len);

            data_len += psh_data_len;

            _rx_event.raise();
        }

        if (flags & tcp_flags_t::TCP_SYN) {
            printk("got a syn\n");

            if (_state != tcp_state_t::TCP_SYN_SENT) {
                _lock.release();
                printk("ret\n");
                return;
            }

            _state = tcp_state_t::TCP_SYN_RECEIVED;

            int option_pos = 20;
            while (option_pos < hdr_len) {
                switch (buffer[option_pos]) {
                case 2: // mss
                    _remote_mss = (uint16_t) * ((big_endian<uint16_t>*) &buffer[option_pos + 2]);
                    _remote_mss = max<int>(10, 1400);

                    option_pos += 4;
                    break;

                default:
                    kpanic("unknown tcp option encountered (%i)", buffer[option_pos]);
                    break;
                }
            }

            _remote_seq = _tcp_header->seq_number;

            data_len++;

            if (_state_state == 1)
                _state = tcp_state_t::TCP_ESTABLISHED;
        }

        if ((uint32_t) _tcp_header->seq_number == _remote_seq)
            _remote_seq += data_len;

        _lock.release();

        if (data_len != 0)
            sendACK((uint32_t) _remote_seq);

        _rx_event.raise();
    }
}