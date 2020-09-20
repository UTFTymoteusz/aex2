#include "socket/inet/tcp.hpp"

#include "aex/assert.hpp"
#include "aex/bool.hpp"
#include "aex/ipc/event.hpp"
#include "aex/mem.hpp"
#include "aex/net.hpp"
#include "aex/printk.hpp"
#include "aex/rwspinlock.hpp"
#include "aex/spinlock.hpp"
#include "aex/sys/time.hpp"

#include "checksum.hpp"
#include "common.hpp"
#include "layer/network/ipv4.hpp"
#include "layer/transport/tcp.hpp"
#include "protocol/inet/tcp.hpp"

#include <stddef.h>
#include <stdint.h>

using namespace AEX;
using namespace AEX::Mem;
using namespace AEX::Net;

// pls make it probe for window updates

namespace NetStack {
    // Need to test these a lil more
    constexpr bool is_before(uint32_t val, uint32_t start, uint32_t extent = 2147483647) {
        if (val == start)
            return false;

        if (val > start)
            return 0xFFFFFFFF - val + start <= extent;
        else if (val < start)
            return start - val <= extent;

        return false;
    }

    constexpr bool is_before_or_equal(uint32_t val, uint32_t start, uint32_t extent = 2147483647) {
        if (val == start)
            return true;

        if (val > start)
            return 0xFFFFFFFF - val + start <= extent;
        else if (val < start)
            return start - val <= extent;

        return false;
    }

    constexpr bool is_after(uint32_t val, uint32_t start, uint32_t extent = 2147483647) {
        return !is_before_or_equal(val, start, extent);
    }

    constexpr bool is_after_or_equal(uint32_t val, uint32_t start, uint32_t extent = 2147483647) {
        return !is_before(val, start, extent);
    }

    char* tcp_debug_serialize_flags(char* buffer, int flags) {
        int ptr = 0;

        const char* bongs[] = {"fin", "syn", "rst", "psh", "ack", "urg"};

        for (int i = 0; i < 6; i++) {
            if (!(flags & (1 << i)))
                continue;

            if (ptr != 0)
                buffer[ptr++] = '/';

            memcpy(buffer + ptr, bongs[i], 3);
            ptr += 3;
        }

        buffer[ptr] = '\0';

        return buffer;
    }

    // un-god this pls
    TCPSocket::TCPSocket(TCPSocket* parent, tcp_listen_entry* listen_entry) {
        auto dev = IPv4Layer::get_interface(listen_entry->source_address);
        AEX_ASSERT(dev);

        this->source_address      = dev->info.ipv4.addr;
        this->source_port         = parent->source_port;
        this->destination_address = listen_entry->source_address;
        this->destination_port    = listen_entry->source_port;

        this->m_block = listen_entry->block;

        printk("a listener hath spawned a new socket: %i.%i.%i.%i:%i >> %i.%i.%i.%i:%i\n",
               source_address[0], source_address[1], source_address[2], source_address[3],
               source_port, destination_address[0], destination_address[1], destination_address[2],
               destination_address[3], destination_port);

        TCPProtocol::pushSocket(this);
    }

    TCPSocket::~TCPSocket() {
        thisSmartPtr->defuse();
        delete thisSmartPtr;

        m_lock.acquire();
        bool send_rst = reset();
        m_lock.release();

        if (send_rst)
            rst();

        TCPProtocol::sockets_lock.acquire_write();

        for (int i = 0; i < TCPProtocol::sockets.count(); i++) {
            if (TCPProtocol::sockets[i] != this)
                continue;

            TCPProtocol::sockets.erase(i);
            break;
        }

        TCPProtocol::sockets_lock.release_write();

        if (source_port != 0)
            TCPProtocol::freePort(source_port);

        printk("tcp socket %i >> %i gone\n", source_port, destination_port);
    }

    error_t TCPSocket::connect(const sockaddr* addr) {
        if (!addr)
            return EINVAL;

        // checks pls
        auto addr_ipv4 = (sockaddr_inet*) addr;

        m_lock.acquire();

        if (m_block.state != TCP_CLOSED) {
            m_lock.release();
            return EISCONN;
        }

        m_async_id++;

        destination_address = addr_ipv4->addr;
        destination_port    = addr_ipv4->port;

        clearState();

        m_block.state = TCP_SYN_SENT;
        m_lock.release();

        syn();

        m_lock.acquire();

        uint64_t timeout_at = Sys::Time::uptime() + TCP_CONNECTION_TIMEOUT;

        while (m_block.state != TCP_ESTABLISHED && m_block.state != TCP_CLOSED &&
               Sys::Time::uptime() < timeout_at) {
            m_rx_event.wait(500);
            m_lock.release();

            Proc::Thread::yield();

            m_lock.acquire();
        }

        if (!equals_one(m_block.state, TCP_ESTABLISHED, TCP_FIN_WAIT_1, TCP_FIN_WAIT_2,
                        TCP_CLOSE_WAIT)) {
            bool send_rst = reset();
            m_lock.release();

            if (send_rst)
                rst();

            return ECONNREFUSED;
        }

        m_lock.release();
        return ENONE;
    }

    error_t TCPSocket::bind(const sockaddr* addr) {
        if (!addr)
            return EINVAL;

        ScopeSpinlock scopeLock(m_lock);

        // checks pls
        auto m_src_addr = (sockaddr_inet*) addr;

        source_address = m_src_addr->addr;
        source_port    = m_src_addr->port;

        return ENONE;
    }

    error_t TCPSocket::listen(int backlog) {
        ScopeSpinlock scopeLock(m_lock);

        if (m_block.state != TCP_CLOSED)
            return EISCONN;

        m_block.state = TCP_LISTEN;

        m_listen_queue   = new Mem::Vector<tcp_listen_entry, 16, 16>;
        m_accept_queue   = new Mem::Vector<Net::Socket_SP, 16, 16>;
        m_listen_backlog = backlog;

        m_tx_buffer.resize(0);
        m_rx_buffer.resize(0);

        return ENONE;
    }

    optional<Net::Socket_SP> TCPSocket::accept() {
        m_lock.acquire();

        if (m_block.state != TCP_LISTEN) {
            m_lock.release();
            return EINVAL;
        }

        while (m_block.state != TCP_CLOSED) {
            m_rx_event.wait();
            m_lock.release();

            Proc::Thread::yield();

            m_lock.acquire();

            if (m_accept_queue->count() == 0)
                continue;

            auto sock = (*m_accept_queue)[0];
            m_accept_queue->erase(0);

            m_listen_backlog++;

            m_lock.release();

            return sock;
        }

        m_lock.release();
        return EINVAL;
    }

    optional<size_t> TCPSocket::sendTo(const void* buffer, size_t len, int,
                                       const sockaddr* dst_addr) {
        if (!buffer || len == 0)
            return EINVAL;

        if (m_block.send_shut)
            return EPIPE;

        if (dst_addr)
            return EISCONN;

        uint8_t* m_buffer = (uint8_t*) buffer;
        size_t   req_len  = len;

        m_lock.acquire();

        if (m_block.state != TCP_ESTABLISHED) {
            m_lock.release();
            return ENOTCONN;
        }

        while (len) {
            // This may still get stuck if you close() and connect() quickly
            if (m_block.send_shut)
                break;

            size_t av = m_tx_buffer.writeAvailable();
            if (av == 0) {
                m_tx_event.wait();
                m_lock.release();

                Proc::Thread::yield();

                m_lock.acquire();
                continue;
            }

            av = min(av, len);

            m_tx_buffer.write(m_buffer, av);

            len -= av;
            m_buffer += av;
        }

        m_lock.release();

        return req_len;
    }

    optional<size_t> TCPSocket::receiveFrom(void* buffer, size_t len, int flags,
                                            sockaddr* src_addr) {
        if (!buffer || len == 0)
            return EINVAL;

        if (src_addr)
            return EISCONN;

        uint8_t* m_buffer = (uint8_t*) buffer;
        size_t   read     = 0;

        m_lock.acquire();

        if (m_block.state != TCP_ESTABLISHED) {
            m_lock.release();
            return ENOTCONN;
        }

        while (len) {
            size_t av = m_rx_buffer.readAvailable();
            if (av == 0) {
                // This may still get stuck if you close() and connect() quickly8
                if ((!(flags & MSG_WAITALL) && read > 0) || m_block.receive_shut)
                    break;

                m_rx_event.wait();
                m_lock.release();

                Proc::Thread::yield();

                m_lock.acquire();
                continue;
            }

            av = min(av, len);

            m_rx_buffer.read(m_buffer, av);

            // clang-format off
            len     -= av;
            m_buffer += av;
            read    += av;
            // clang-format on

            if (m_block.notify_of_window_size) {
                m_block.notify_of_window_size = false;

                m_lock.release();
                windowUpdate();
                m_lock.acquire();
            }
        }

        m_lock.release();

        return read;
    }

    void TCPSocket::clearState() {
        m_tx_buffer.resize(TCP_TX_BUFFER_SIZE);
        m_rx_buffer.resize(TCP_RX_BUFFER_SIZE);

        m_retransmission_queue.clear();
        m_block.state = TCP_CLOSED;

        m_block.snd_una = 0;
        m_block.snd_nxt = 0;
        m_block.snd_wnd = 0;
        m_block.snd_mss = TCP_MIN_MSS;

        m_block.rcv_nxt = 0;

        m_block.sending_comp_ack      = false;
        m_block.notify_of_window_size = false;
        m_block.closing               = false;

        m_block.send_shut    = false;
        m_block.receive_shut = false;
    }

    void TCPSocket::tick() {
        if (m_block.state == TCP_CLOSED)
            return;

        package();
        retransmit();
    }

    void TCPSocket::syn() {
        // I need a random number generator
        auto uptime = Sys::Time::uptime();

        m_block.snd_nxt = (uptime - (uptime % 10000)) + 2137;
        m_block.snd_mss = TCP_MIN_MSS;

        auto seg = new segment();

        seg->seq        = m_block.snd_nxt;
        seg->flags      = TCP_SYN;
        seg->header_len = 24;
        seg->len        = 4;
        seg->data       = new uint8_t[4];

        seg->data[0] = 0x02;
        seg->data[1] = 0x04;
        seg->data[2] = 0x05; // yeees, hardcoded 1460, I'm too lazy
        seg->data[3] = 0xb4;

        seg->retransmit_at = Sys::Time::uptime() + TCP_RETRANSMIT_INTERVAL;

        m_block.snd_una = m_block.snd_nxt;
        m_block.snd_nxt++;

        seg->ack_with = m_block.snd_nxt;

        m_lock.acquire();
        m_retransmission_queue.push(seg);
        m_lock.release();

        sendSegment(seg);
    }

    void TCPSocket::ack() {
        m_block.sending_comp_ack = true;
        return;
    }

    void TCPSocket::ack(uint32_t manual) {
        auto seg = segment();

        seg.seq        = m_block.snd_nxt;
        seg.ack        = manual;
        seg.flags      = TCP_ACK;
        seg.header_len = 20;
        seg.len        = 0;

        sendSegment(&seg);
    }

    void TCPSocket::rst() {
        auto seg = segment();

        if (m_block.state == TCP_SYN_RECEIVED)
            seg.ack = m_block.rcv_nxt;

        seg.seq = m_block.snd_nxt;

        seg.flags      = TCP_RST;
        seg.header_len = 20;
        seg.len        = 0;

        sendSegment(&seg);
    }

    void TCPSocket::fin() {
        auto seg = new segment();

        seg->seq        = m_block.snd_nxt;
        seg->ack        = m_block.rcv_nxt;
        seg->flags      = (tcp_flags_t)(TCP_FIN | TCP_ACK);
        seg->header_len = 20;
        seg->len        = 0;

        seg->retransmit_at = Sys::Time::uptime() + TCP_RETRANSMIT_INTERVAL;

        m_block.sending_comp_ack = false;
        m_block.snd_nxt++;

        seg->ack_with = m_block.snd_nxt;

        m_lock.acquire();
        m_retransmission_queue.push(seg);
        m_lock.release();

        sendSegment(seg);
    }

    bool TCPSocket::reset() {
        bool verdict = true;

        if (m_block.closing || equals_one(m_block.state, TCP_LISTEN, TCP_CLOSED))
            verdict = false;

        m_block.closing = true;
        closeInternal();

        return verdict;
    }

    void TCPSocket::windowUpdate() {
        auto seg = segment();

        seg.seq        = m_block.snd_nxt;
        seg.ack        = m_block.rcv_nxt;
        seg.flags      = TCP_ACK;
        seg.header_len = 20;
        seg.len        = 0;

        sendSegment(&seg);
    }

    int TCPSocket::getWindow() {
        int window = (TCP_RX_BUFFER_SIZE - 1) - m_rx_buffer.readAvailable();
        window     = max(0, window - TCP_PROBE_SLACK);

        return window;
    }

    tcp_listen_entry* TCPSocket::tryCreateListenEntry(Net::ipv4_addr addr, uint16_t port) {
        int count = m_listen_queue->count();

        for (int i = 0; i < count; i++) {
            auto entry = (*m_listen_queue)[i];
            if (entry.source_address == addr && entry.source_port == port)
                return nullptr;
        }

        auto entry = tcp_listen_entry();

        entry.source_address = addr;
        entry.source_port    = port;

        m_listen_queue->push(entry);

        return &(*m_listen_queue)[m_listen_queue->count() - 1];
    }

    tcp_listen_entry* TCPSocket::getListenEntry(Net::ipv4_addr addr, uint16_t port) {
        int count = m_listen_queue->count();

        for (int i = 0; i < count; i++) {
            auto entry = (*m_listen_queue)[i];
            if (entry.source_address != addr || entry.source_port != port)
                continue;

            return &(*m_listen_queue)[i];
        }

        return nullptr;
    }

    void TCPSocket::removeEntry(Net::ipv4_addr addr, uint16_t port) {
        int count = m_listen_queue->count();

        for (int i = 0; i < count; i++) {
            auto entry = (*m_listen_queue)[i];
            if (entry.source_address != addr || entry.source_port != port)
                continue;

            m_listen_queue->erase(i);

            count--;
            i--;
        }
    }

    bool TCPSocket::sendSegment(segment* seg) {
        return sendSegment(seg, destination_address, destination_port);
    }

    bool TCPSocket::sendSegment(segment* seg, Net::ipv4_addr addr, uint16_t port) {
        auto dev = IPv4Layer::get_interface(addr);
        if (!dev && m_block.state == TCP_CLOSED) {
            if (reset())
                rst();

            return false;
        }

        uint8_t  packet[IPv4Layer::encaps_len(sizeof(tcp_header) + seg->len)];
        uint8_t* payload =
            IPv4Layer::encapsulate(packet, sizeof(tcp_header) + seg->len, addr, IPv4_TCP);

        auto m_tcp_header = (tcp_header*) payload;

        m_tcp_header->seq_number          = seg->seq;
        m_tcp_header->ack_number          = seg->ack;
        m_tcp_header->source_port         = this->source_port;
        m_tcp_header->destination_port    = port;
        m_tcp_header->checksum            = 0x0000;
        m_tcp_header->urgent_pointer      = 0;
        m_tcp_header->goddamned_bitvalues = ((seg->header_len / 4) << 12) | seg->flags;
        m_tcp_header->window              = getWindow();

        auto m_fake_header = tcp_fake_ipv4_header();

        m_fake_header.source      = dev->info.ipv4.addr;
        m_fake_header.destination = addr;
        m_fake_header.zero        = 0x00;
        m_fake_header.protocol    = IPv4_TCP;
        m_fake_header.length      = seg->len + sizeof(tcp_header);

        uint32_t sum = sum_bytes(&m_fake_header, sizeof(tcp_fake_ipv4_header)) +
                       sum_bytes(m_tcp_header, sizeof(tcp_header)) + sum_bytes(seg->data, seg->len);
        m_tcp_header->checksum = to_checksum(sum);

        memcpy(payload + sizeof(tcp_header), seg->data, seg->len);

        if constexpr (TCPConfig::FLAG_PRINT) {
            char buffer[32];
            tcp_debug_serialize_flags(buffer, seg->flags);

            printk("tcp: %5i >> %5i <%s>\n", this->source_port, port, buffer);
        }

        IPv4Layer::route(packet, IPv4Layer::encaps_len(sizeof(tcp_header) + seg->len));
        return true;
    }

    void TCPSocket::packetReceived(Net::ipv4_addr src, uint16_t src_port, const uint8_t* buffer,
                                   uint16_t len) {
        if (m_block.state != TCP_LISTEN && src != destination_address &&
            src_port != destination_port)
            return;

        if (m_block.state == TCP_CLOSED)
            return;

        if (m_block.state == TCP_LISTEN) {
            packetReceivedListen(src, src_port, buffer, len);
            return;
        }

        m_lock.acquire();

        auto m_tcp_header = (tcp_header*) buffer;

        uint16_t    hdr_len = ((((uint16_t) m_tcp_header->goddamned_bitvalues) >> 12) * 4);
        tcp_flags_t flags   = (tcp_flags_t)((uint16_t) m_tcp_header->goddamned_bitvalues);

        uint32_t seqn = (uint32_t) m_tcp_header->seq_number;
        uint32_t ackn = (uint32_t) m_tcp_header->ack_number;

        uint32_t expected_seqn = m_block.rcv_nxt;

        uint16_t data_len = len - hdr_len;
        if (hdr_len >= len)
            data_len = 0;

        readOptions(&m_block, buffer, hdr_len);

        if (flags & TCP_RST) {
            if (m_block.state == TCP_SYN_SENT && (flags & TCP_ACK) && ackn == m_block.snd_nxt)
                reset();
            else if (is_after_or_equal(seqn, expected_seqn) &&
                     is_before(seqn, expected_seqn + getWindow()))
                reset();

            m_lock.release();
            return;
        }

        if (flags & TCP_SYN) {
            if (m_block.state == TCP_SYN_SENT) {
                m_block.rcv_nxt = m_tcp_header->seq_number;
                m_block.rcv_nxt++;

                m_lock.release();
                ack(m_block.rcv_nxt);
                m_lock.acquire();

                m_block.state = TCP_SYN_RECEIVED;
                m_rx_event.raise();
            }
        }

        if (flags & TCP_ACK) {
            if (is_after_or_equal(ackn, m_block.snd_una)) {
                m_block.snd_una = ackn;
                m_block.snd_wnd = m_tcp_header->window;
            }

            if (equals_one(m_block.state, TCP_SYN_SENT, TCP_SYN_RECEIVED)) {
                // This is a thing because debugging got annoying
                // Also need to read up on the RFC to see if this is fair
                if (ackn != m_block.snd_nxt) {
                    m_block.snd_nxt = ackn;
                    m_block.rcv_nxt = seqn;

                    bool send_rst = reset();
                    m_lock.release();

                    if (send_rst)
                        rst();

                    return;
                }
            }

            int count = m_retransmission_queue.count();

            for (int i = 0; i < count; i++) {
                auto seg = m_retransmission_queue[i];
                if (!is_before_or_equal(seg->ack_with, ackn))
                    continue;

                if (seg->flags & TCP_FIN) {
                    if (m_block.state == TCP_FIN_WAIT_1) {
                        m_block.state = TCP_FIN_WAIT_2;

                        m_rx_event.raise();
                    }
                    else if (m_block.state == TCP_LAST_ACK || m_block.state == TCP_CLOSING) {
                        closeInternal();
                        m_block.state = TCP_CLOSED; // Cleaner this way
                        m_lock.release();

                        return;
                    }
                }
                else if (seg->flags & TCP_SYN) {
                    if (m_block.state == TCP_SYN_RECEIVED) {
                        m_block.state = TCP_ESTABLISHED;

                        m_rx_event.raise();
                    }
                }

                m_retransmission_queue.erase(i);

                count--;
                i--;
            }
        }

        // I need reordering
        if (data_len) {
            if (seqn == expected_seqn) {
                if (data_len > m_rx_buffer.writeAvailable()) {
                    bool send_rst = reset();
                    m_lock.release();

                    if (send_rst)
                        rst();

                    return;
                }

                m_block.rcv_nxt += data_len;

                if (!m_block.receive_shut)
                    m_rx_buffer.write(buffer + hdr_len, data_len);

                ack();

                if (getWindow() < TCP_RX_BUFFER_SIZE / 2)
                    m_block.notify_of_window_size = true;

                m_rx_event.raise();
            }
            else if (is_before_or_equal(seqn + data_len, m_block.rcv_nxt)) {
                m_lock.release();
                ack(seqn + data_len);
                m_lock.acquire();
            }
        }

        // I need reordering
        if (flags & TCP_FIN && seqn == expected_seqn) {
            m_block.receive_shut = true;

            if (m_block.state == TCP_FIN_WAIT_1) {
                m_block.rcv_nxt++;
                ack();

                m_block.state = TCP_CLOSING;
            }
            else if (m_block.state == TCP_FIN_WAIT_2) {
                m_block.rcv_nxt++;
                ack();
            }
            else if (m_block.state == TCP_ESTABLISHED) {
                m_block.rcv_nxt++;
                ack();

                m_block.state = TCP_CLOSE_WAIT;
            }

            m_rx_event.raise();
        }

        m_lock.release();
    }

    void TCPSocket::packetReceivedListen(Net::ipv4_addr src, uint16_t src_port,
                                         const uint8_t* buffer, uint16_t len) {
        auto m_tcp_header = (tcp_header*) buffer;

        uint16_t    hdr_len = ((((uint16_t) m_tcp_header->goddamned_bitvalues) >> 12) * 4);
        tcp_flags_t flags   = (tcp_flags_t)((uint16_t) m_tcp_header->goddamned_bitvalues);

        uint32_t seqn = (uint32_t) m_tcp_header->seq_number;
        uint32_t ackn = (uint32_t) m_tcp_header->ack_number;

        uint32_t expected_seqn = m_block.rcv_nxt;

        uint16_t data_len = len - hdr_len;
        if (hdr_len >= len)
            data_len = 0;

        m_lock.acquire();

        if (flags & TCP_RST) {
            m_lock.release();
            return;
        }

        if (flags & TCP_SYN) {
            auto new_entry = tryCreateListenEntry(src, src_port);

            if (new_entry) {
                // I need a random number generator
                auto uptime = Sys::Time::uptime();

                new_entry->block.snd_nxt = (uptime - (uptime % 10000)) + 2137 + 1;
                new_entry->block.snd_mss = TCP_MIN_MSS;
                new_entry->block.snd_wnd = (uint16_t) m_tcp_header->window;
                new_entry->block.snd_una = new_entry->block.snd_nxt;
                new_entry->block.rcv_nxt = m_tcp_header->seq_number + 1;
                new_entry->block.state   = TCP_SYN_RECEIVED;
            }
            else
                new_entry = getListenEntry(src, src_port);

            if (!new_entry) {
                m_lock.release();
                return;
            }

            auto seg = segment();

            seg.seq        = new_entry->block.snd_nxt - 1;
            seg.ack        = new_entry->block.rcv_nxt;
            seg.flags      = (tcp_flags_t)(TCP_SYN | TCP_ACK);
            seg.header_len = 24;
            seg.len        = 4;
            seg.data       = new uint8_t[4];

            seg.data[0] = 0x02;
            seg.data[1] = 0x04;
            seg.data[2] = 0x05; // yeees, hardcoded 1460, I'm too lazy
            seg.data[3] = 0xb4;

            m_lock.release();
            sendSegment(&seg, src, src_port);
            m_lock.acquire();
        }

        auto* entry = getListenEntry(src, src_port);
        if (!entry) {
            m_lock.release();
            return;
        }

        // pls make it read data from the pre-synced states as that's legal too

        readOptions(&entry->block, buffer, hdr_len);

        if (flags & TCP_ACK) {
            if (entry->block.state == TCP_SYN_RECEIVED) {
                if (is_after_or_equal(ackn, entry->block.snd_nxt) && m_listen_backlog > 0) {
                    entry->block.state = TCP_ESTABLISHED;

                    auto sock = new TCPSocket(this, entry);
                    m_accept_queue->push(*sock->thisSmartPtr);

                    removeEntry(src, src_port);

                    m_rx_event.raise();

                    m_listen_backlog--;
                }
            }
        }

        m_lock.release();
    }

    void TCPSocket::closeInternal() {
        m_block.state = TCP_CLOSED;

        m_block.send_shut    = true;
        m_block.receive_shut = true;

        m_tx_event.raise();
        m_rx_event.raise();

        if (m_listen_queue) {
            delete m_listen_queue;
            m_listen_queue = nullptr;
        }

        if (m_accept_queue) {
            delete m_accept_queue;
            m_accept_queue = nullptr;
        }

        m_retransmission_queue.clear();
    }

    void TCPSocket::package() {
        auto uptime = Sys::Time::uptime();

        m_lock.acquire();

        while (m_tx_buffer.readAvailable()) {
            AEX_ASSERT(m_block.snd_wnd != 0);

            int size =
                min(m_tx_buffer.readAvailable(), (int) m_block.snd_wnd, (int) m_block.snd_mss);

            auto seg = new segment();

            seg->seq        = m_block.snd_nxt;
            seg->ack        = m_block.rcv_nxt;
            seg->flags      = (tcp_flags_t)(TCP_PSH | TCP_ACK);
            seg->header_len = 20;
            seg->len        = size;
            seg->data       = new uint8_t[size];

            seg->retransmit_at = uptime + TCP_RETRANSMIT_INTERVAL;

            m_tx_buffer.read(seg->data, size);

            m_block.snd_nxt += size;
            m_block.snd_wnd -= size;

            seg->ack_with = m_block.snd_nxt;

            m_retransmission_queue.push(seg);

            m_lock.release();
            sendSegment(seg);
            m_lock.acquire();

            m_block.sending_comp_ack = false;
        }

        m_lock.release();

        if (m_block.sending_comp_ack) {
            ack(m_block.rcv_nxt);
            m_block.sending_comp_ack = false;
        }
    }

    void TCPSocket::retransmit() {
        auto uptime = Sys::Time::uptime();

        m_lock.acquire();
        int count = m_retransmission_queue.count();

        for (int i = 0; i < count; i++) {
            auto seg = m_retransmission_queue[i];

            if (uptime < seg->retransmit_at)
                continue;

            seg->retries++;
            if (seg->retries > TCP_RETRIES) {
                bool send_rst = reset();
                m_lock.release();

                if (send_rst)
                    rst();

                return;
            }

            seg->retransmit_at += TCP_RETRANSMIT_INTERVAL;

            m_lock.release();
            sendSegment(seg);
            m_lock.acquire();
        }

        m_tx_event.raise();
        m_lock.release();
    }

    void TCPSocket::readOptions(tcp_block* block, const uint8_t* buffer, uint16_t hdr_len) {
        for (int i = sizeof(tcp_header); i < hdr_len;) {
            uint8_t kind = buffer[i + 0];
            uint8_t len  = buffer[i + 1];

            if (len < 2)
                len = 2;

            switch (kind) {
            case TCP_OPT_END:
                i = 65536;
                break;
            case TCP_OPT_NOOP:
                i++;
                return;
            case TCP_OPT_MSS: {
                auto mss       = (big_endian<uint16_t>*) &buffer[i + 2];
                block->snd_mss = clamp((uint16_t) *mss, (uint16_t) TCP_MIN_MSS, (uint16_t) 1460);
            } break;
            default:
                break;
            }

            i += len;
        }
    }

    error_t TCPSocket::shutdown(int how) {
        if (how == 0)
            return EINVAL;

        m_lock.acquire();

        if (equals_one(m_block.state, TCP_LISTEN, TCP_SYN_SENT, TCP_SYN_RECEIVED, TCP_CLOSED)) {
            m_lock.release();
            return ENOTCONN;
        }

        if (how & SHUT_RD && !m_block.receive_shut)
            m_block.receive_shut = true;

        if (how & SHUT_WR && !m_block.send_shut) {
            m_block.send_shut = true;

            while (m_tx_buffer.readAvailable() || m_block.snd_una != m_block.snd_nxt) {
                m_tx_event.wait();
                m_lock.release();

                Proc::Thread::yield();

                m_lock.acquire();
            }

            if (m_block.state == TCP_ESTABLISHED)
                m_block.state = TCP_FIN_WAIT_1;
            else if (m_block.state == TCP_CLOSE_WAIT)
                m_block.state = TCP_LAST_ACK;

            m_lock.release();
            fin();
            m_lock.acquire();

            while (!equals_one(m_block.state, TCP_CLOSED)) {
                m_rx_event.wait();
                m_lock.release();

                Proc::Thread::yield();

                m_lock.acquire();
            }
        }

        m_lock.release();

        return ENONE;
    }

    // I need to make it send FIN on SYN_RECEIVED
    error_t TCPSocket::close() {
        m_lock.acquire();

        if (m_block.state == TCP_CLOSE_WAIT)
            fin();
        else if (equals_one(m_block.state, TCP_LAST_ACK, TCP_CLOSED))
            ;
        else {
            bool send_rst = reset();
            m_lock.release();

            if (send_rst)
                rst();

            m_lock.acquire();
        }

        while (!equals_one(m_block.state, TCP_CLOSED)) {
            m_rx_event.wait();
            m_lock.release();

            Proc::Thread::yield();

            m_lock.acquire();
        }

        m_lock.release();

        return ENONE;
    }
}