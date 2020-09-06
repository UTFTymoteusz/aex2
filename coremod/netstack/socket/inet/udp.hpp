#pragma once

#include "aex/ipc/event.hpp"
#include "aex/net.hpp"
#include "aex/spinlock.hpp"

#include <stddef.h>
#include <stdint.h>

namespace NetStack {
    class UDPProtocol;

    class UDPSocket : public AEX::Net::Socket {
        public:
        AEX::Net::ipv4_addr source_address;
        uint16_t            source_port = 0;

        AEX::Net::ipv4_addr destination_address;
        uint16_t            destination_port = 0;

        ~UDPSocket();

        AEX::error_t connect(const AEX::Net::sockaddr* addr);
        AEX::error_t bind(const AEX::Net::sockaddr* addr);

        AEX::optional<size_t> sendTo(const void* buffer, size_t len, int flags,
                                     const AEX::Net::sockaddr* dst_addr);
        AEX::optional<size_t> receiveFrom(void* buffer, size_t len, int flags,
                                          AEX::Net::sockaddr* src_addr);

        private:
        struct datagram {
            datagram* next = nullptr;

            AEX::Net::ipv4_addr source_address;
            uint16_t            source_port;

            int     len;
            uint8_t buffer[];
        };

        size_t m_buffered_size = 0;

        datagram* m_first_datagram = nullptr;
        datagram* m_last_datagram  = nullptr;

        AEX::Spinlock         m_lock;
        AEX::IPC::SimpleEvent m_event;

        bool m_non_blocking = false;

        void packetReceived(AEX::Net::ipv4_addr src, uint16_t src_port, const uint8_t* buffer,
                            uint16_t len);

        friend class UDPProtocol;
    };
}