#pragma once

#include "aex/spinlock.hpp"

#include <stdint.h>

namespace AEX::NetStack {
    // add overflow checks pls
    struct packet_buffer {
        Spinlock lock;

        uint16_t pos = 0;
        uint8_t  bytes[1768];

        void reset() {
            pos = 0;
        }

        void release() {
            lock.release();
        }

        void* alloc(int len) {
            int cpos = pos;

            pos += len;
            return (void*) &bytes[cpos];
        }

        void write(const void* data, int len) {
            memcpy(alloc(len), data, len);
        }

        void* get() {
            return (void*) bytes;
        }

        int length() {
            return pos;
        }
    };
}