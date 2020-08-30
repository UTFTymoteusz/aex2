#pragma once

#include <stddef.h>
#include <stdint.h>

namespace NetStack {
    struct packet_buffer;

    void rx_init();

    void queue_rx_packet(int device_id, const void* data, uint16_t len);
}
