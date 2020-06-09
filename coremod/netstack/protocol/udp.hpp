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
    static constexpr auto SOCKET_BUFFER_SIZE = 1048576;

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
}