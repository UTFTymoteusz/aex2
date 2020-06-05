#pragma once

#include "aex/dev/net.hpp"
#include "aex/mem/smartptr.hpp"
#include "aex/net/ethernet.hpp"

#include "layer/ethernet.hpp"

namespace AEX::NetProto::NetCore {
    Mem::SmartPointer<Dev::Net> get_interface(Net::mac_addr mac);

    packet_buffer* get_tx_buffer();

    void queue_tx_packet(ethertype_t type, const void* data, uint16_t len);
    void queue_rx_packet(int device_id, ethertype_t type, const void* data, uint16_t len);

    void init();
}