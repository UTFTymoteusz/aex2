#pragma once

#include "aex/errno.hpp"
#include "aex/ipc/event.hpp"
#include "aex/mem.hpp"
#include "aex/net.hpp"
#include "aex/net/inetprotocol.hpp"
#include "aex/spinlock.hpp"

#include <stdint.h>

namespace AEX::NetStack {
    static constexpr auto TCP_SOCKET_BUFFER_SIZE = 65536;

    class TCPSocket;

    class TCPProtocol : public Net::INetProtocol {
        public:
        static Spinlock                        sockets_lock;
        static Mem::Vector<TCPSocket*, 32, 32> sockets;

        static void init();
        static void loop();

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

        static Mem::SmartPointer<Proc::Thread> _loop_thread;
    };
}