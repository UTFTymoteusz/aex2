#pragma once

#include <stddef.h>
#include <stdint.h>

namespace NetStack {
    // add overflow checks pls
    struct packet {
        uint8_t* buffer;
        uint16_t len;
        uint16_t pos;

        packet(uint16_t len) {
            this->buffer = new uint8_t[len];
            this->len    = len;
            this->pos    = 0;
        }
    };
}