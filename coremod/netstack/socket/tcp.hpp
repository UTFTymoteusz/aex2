#pragma once

#include "aex/ipc/event.hpp"
#include "aex/mem/circularbuffer.hpp"
#include "aex/net/socket.hpp"
#include "aex/spinlock.hpp"

#include <stddef.h>
#include <stdint.h>

namespace AEX::NetStack {
    static constexpr auto TCP_RETRANSMIT_INTERVAL = 5ul * 1000000000;

    enum tcp_state_t {
        TCP_CLOSED = 0,
        TCP_SYN_SENT,
        TCP_SYN_RECEIVED,
        TCP_ESTABLISHED,
    };

    enum tcp_flags_t {
        TCP_FIN = 0x01,
        TCP_SYN = 0x02,
        TCP_RST = 0x04,
        TCP_PSH = 0x08,
        TCP_ACK = 0x10,
        TCP_URG = 0x20,
    };

    class TCPProtocol;

    class TCPSocket : public Net::Socket {
        public:
        Net::ipv4_addr source_address;
        uint16_t       source_port = 0;

        Net::ipv4_addr destination_address;
        uint16_t       destination_port = 0;

        ~TCPSocket();

        error_t connect(const Net::sockaddr* addr);
        error_t bind(const Net::sockaddr* addr);

        optional<size_t> sendTo(const void* buffer, size_t len, int flags,
                                const Net::sockaddr* dst_addr);
        optional<size_t> receiveFrom(void* buffer, size_t len, int flags, Net::sockaddr* src_addr);

        private:
        struct segment {
            segment* next;
            segment* prev;

            int         seq;
            int         expected_ack;
            int         data_offset;
            tcp_flags_t flags;

            uint64_t retransmit_at;
            uint8_t  retries;

            int     len;
            uint8_t buffer[];

            segment() {}
        };

        Mem::CircularBuffer _tx_circ_buffer = Mem::CircularBuffer(8192);
        Mem::CircularBuffer _rx_circ_buffer = Mem::CircularBuffer(8192);

        segment* _first_segment = nullptr;
        segment* _last_segment  = nullptr;

        Spinlock         _lock;
        IPC::SimpleEvent _tx_event;
        IPC::SimpleEvent _rx_event;

        bool        _non_blocking = false;
        tcp_state_t _state        = tcp_state_t::TCP_CLOSED;
        int         _state_state  = 0;

        int _current_seq = 2137;
        int _remote_seq  = 0;

        int _remote_mss = 1400;

        void tick();

        void pushSegment(segment* segment);
        void sendSegment(segment* segment, int ack = 0);

        void sendACK(int to_ack);
        void sendRST();

        void packetReceived(Net::ipv4_addr src, uint16_t src_port, uint8_t* buffer, uint16_t len);

        friend class TCPProtocol;
    };
}