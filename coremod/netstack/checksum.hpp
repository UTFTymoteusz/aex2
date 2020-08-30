#pragma once

#include "aex/byte.hpp"

#include <stdint.h>

namespace NetStack {
    inline uint32_t sum_bytes(const void* buffer, uint16_t len) {
        uint32_t  total = 0;
        uint16_t* bong  = (uint16_t*) buffer;

        for (uint16_t i = 0; i < len / 2; i++)
            total += bong[i];

        if (len & 0x01)
            total += *((uint8_t*) buffer + (len & ~0x01));

        return total;
    }

    inline uint16_t to_checksum(uint32_t total) {
        while (total >> 16)
            total = (total & 0xFFFF) + (total >> 16);

        return AEX::from_big_endian<uint16_t>((uint16_t) ~total);
    }
}
