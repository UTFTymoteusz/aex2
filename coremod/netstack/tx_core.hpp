#pragma once

#include "aex/dev.hpp"
#include "aex/mem.hpp"
#include "aex/net.hpp"
#include "aex/net/ethernet.hpp"

#include "layer/ethernet.hpp"

#include <stddef.h>
#include <stdint.h>

namespace AEX::NetStack {
    struct packet_buffer;

    void tx_init();

    packet_buffer* get_tx_buffer();

    void queue_tx_packet(const void* data, uint16_t len);
    void queue_tx_packet(Dev::NetDevice_SP net_dev, const void* data, uint16_t len);

    Dev::NetDevice_SP get_interface_by_srcaddr(Net::mac_addr mac);
    Dev::NetDevice_SP get_interface_by_srcaddr(Net::ipv4_addr ipv4_addr);

    Dev::NetDevice_SP get_interface_for_dst(Net::ipv4_addr ipv4_addr);
}
