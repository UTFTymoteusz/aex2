#pragma once

#include "aex/errno.hpp"
#include "aex/ipc/event.hpp"
#include "aex/mem/vector.hpp"
#include "aex/net/inetprotocol.hpp"
#include "aex/net/ipv4.hpp"
#include "aex/net/socket.hpp"
#include "aex/spinlock.hpp"

#include <stdint.h>

namespace AEX::NetStack {
    class UDPSocket;

    class UDPProtocol : public Net::INetProtocol {
        public:
        static Spinlock                        sockets_lock;
        static Mem::Vector<UDPSocket*, 32, 32> sockets;

        static void init();

        optional<Net::Socket*> createSocket(Net::socket_type_t type);

        static void packetReceived(Net::ipv4_addr src, uint16_t src_port, Net::ipv4_addr dst,
                                   uint16_t dst_port, uint8_t* buffer, uint16_t len);

        static uint16_t allocateDynamicPort();
        static bool     allocatePort(uint16_t port);
        static void     freePort(uint16_t port);

        private:
        static Spinlock  _ports_lock;
        static uint32_t* _port_bitmap;
        static uint16_t  _port_dynamic_last;
    };

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