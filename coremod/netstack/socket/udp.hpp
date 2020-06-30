#pragma once

#include "aex/ipc/event.hpp"
#include "aex/net.hpp"
#include "aex/spinlock.hpp"

#include <stddef.h>
#include <stdint.h>

namespace AEX::NetStack {
    class UDPProtocol;

    class UDPSocket : public Net::Socket {
        public:
        Net::ipv4_addr source_address;
        uint16_t       source_port = 0;

        Net::ipv4_addr destination_address;
        uint16_t       destination_port = 0;

        ~UDPSocket();

        error_t connect(const Net::sockaddr* addr);
        error_t bind(const Net::sockaddr* addr);

        optional<size_t> sendTo(const void* buffer, size_t len, int flags,
                                const Net::sockaddr* dst_addr);
        optional<size_t> receiveFrom(void* buffer, size_t len, int flags, Net::sockaddr* src_addr);

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

        bool _non_blocking = false;

        void packetReceived(Net::ipv4_addr src, uint16_t src_port, uint8_t* buffer, uint16_t len);

        friend class UDPProtocol;
    };
}