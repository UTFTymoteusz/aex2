#include "layer/none.hpp"

#include "rx_core.hpp"
#include "tx_core.hpp"

namespace AEX::NetStack {
    error_t NoneLayer::parse(int device_id, const void* packet_ptr, size_t len) {
        ethertype_t ethertype = *((ethertype_t*) packet_ptr);

        packet_ptr += sizeof(ethertype_t);
        len -= sizeof(ethertype_t);

        queue_rx_packet(device_id, ethertype, packet_ptr, len);
        return error_t::ENONE;
    }

    packet_buffer* NoneLayer::encapsulate(ethertype_t type) {
        auto buffer = get_tx_buffer();

        ethertype_t* ethertype = (ethertype_t*) buffer->alloc(sizeof(ethertype_t));
        *ethertype             = type;

        return buffer;
    }
}