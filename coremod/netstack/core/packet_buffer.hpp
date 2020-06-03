#pragma once

#include "aex/spinlock.hpp"

#include <stdint.h>

namespace AEX::NetProto::NetCore {
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

        void* get() {
            return (void*) bytes;
        }

        int length() {
            return pos;
        }
    };
}