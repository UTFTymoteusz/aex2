#pragma once

#include "aex/dev/netdevice.hpp"
#include "aex/errno.hpp"
#include "aex/proc/thread.hpp"

#include "netstack/ipv4.hpp"
#include "netstack/packet.hpp"

#include <stdint.h>

namespace NetStack {
    class IPv4Layer {
        public:
        static constexpr auto MAX_RETX_QUEUE_SIZE = 65536;
        static constexpr auto MAX_RETX_TRIES      = 8;

        static void init();

        static int      encaps_len(int payload_len);
        static uint8_t* encapsulate(uint8_t* buffer, uint16_t len, AEX::Net::ipv4_addr dst,
                                    ipv4_protocol_t protocol);
        static void     parse(AEX::Dev::NetDevice_SP dev, const uint8_t* buffer, uint16_t len);

        static AEX::error_t route(const uint8_t* buffer, uint16_t len);

        static AEX::Dev::NetDevice_SP get_interface(AEX::Net::ipv4_addr ipv4_addr);

        private:
        struct retx_frame {
            uint8_t tries = 0;

            uint8_t* data = nullptr;
            uint16_t len  = 0;
        };

        static AEX::Mem::Vector<retx_frame, 16, 16> m_retx_queue;
        static AEX::Spinlock                        m_retx_queue_lock;
        static uint32_t                             m_retx_queue_size;

        static AEX::Proc::Thread* m_loop_thread;
        static AEX::IPC::Event    m_loop_event;

        static void loop();
        static bool retx();

        static void retx_add(const uint8_t* buffer, uint16_t len);

        static AEX::error_t send(const uint8_t* buffer, uint16_t len);
    };
}