#pragma once

#include "netstack/packet.hpp"

namespace NetStack {
    void arp_received(int device_id, const uint8_t* buffer, uint16_t len);
    void ipv4_received(int device_id, const uint8_t* buffer, uint16_t len);
    void ipv6_received(int device_id, const uint8_t* buffer, uint16_t len);
}