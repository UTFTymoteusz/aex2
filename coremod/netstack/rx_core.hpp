#pragma once

#include "layer/ethernet.hpp"

#include <stddef.h>
#include <stdint.h>

namespace AEX::NetStack {
    struct packet_buffer;

    void rx_init();

    void queue_rx_packet(int device_id, ethertype_t type, const void* data, uint16_t len);
}
