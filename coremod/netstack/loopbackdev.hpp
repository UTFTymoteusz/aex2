#pragma once

#include "aex/dev/netdevice.hpp"
#include "aex/net/linklayer.hpp"

namespace AEX::NetStack {
    class Loopback : public Dev::NetDevice {
        public:
        Loopback() : Dev::NetDevice("lo", Net::link_type_t::LINK_NONE) {}

        error_t send(const void* buffer, size_t len) {
            receive(buffer, len);
            return error_t::ENONE;
        }
    };
}