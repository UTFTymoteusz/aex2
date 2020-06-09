#pragma once

#include "aex/ipc/event.hpp"
#include "aex/net/socket.hpp"
#include "aex/spinlock.hpp"

#include <stddef.h>
#include <stdint.h>

namespace AEX::NetStack {
    class UDPProtocol;

    class UDPSocket : public Net::Socket {
        public:
        // bool active = false;

        Net::ipv4_addr source_address;
        uint16_t       source_port = 0;

        ~UDPSocket();

        error_t bind(const Net::sockaddr* addr);

        optional<size_t> sendto(const void* buffer, size_t len, int flags,
                                const Net::sockaddr* dst_addr);
        optional<size_t> recvfrom(void* buffer, size_t len, int flags, Net::sockaddr* src_addr);

        private:
        struct datagram {
            datagram* next = nullptr;

            Net::ipv4_addr source_address;
            uint16_t       source_port;

            int     len;
            uint8_t buffer[];
        };

        size_t _buffered_size = 0;

        datagram* _first_datagram = nullptr;
        datagram* _last_datagram  = nullptr;

        Spinlock         _lock;
        IPC::SimpleEvent _event;

        void packetReceived(Net::ipv4_addr src, uint16_t src_port, uint8_t* buffer, uint16_t len);

        uint16_t allocatePort();
        void     releasePort(uint16_t port);

        friend class UDPProtocol;
    };
}