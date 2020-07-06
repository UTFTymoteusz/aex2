#pragma once

#include "aex/dev.hpp"
#include "aex/net/linklayer.hpp"

namespace AEX::NetStack {
    class Loopback : public Dev::NetDevice {
        public:
        Loopback() : Dev::NetDevice("lo", Net::LINK_NONE) {}

        error_t send(const void* buffer, size_t len) {
            receive(buffer, len);
            return ENONE;
        }
    };
}