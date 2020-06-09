#pragma once

#include "aex/net/linklayer.hpp"

#include "layer/ethernet.hpp"

namespace AEX::NetStack {
    class NoneLayer : public Net::LinkLayer {
        public:
        error_t parse(int device_id, const void* packet_ptr, size_t len);

        static packet_buffer* encapsulate(ethertype_t type);

        private:
    };
}