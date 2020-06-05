#include "layer/ipv4.hpp"

#include "aex/debug.hpp"
#include "aex/printk.hpp"


namespace AEX::NetProto {
    const Net::ipv4_addr IPv4Layer::BROADCAST_ADDR = Net::ipv4_addr(0xFF, 0xFF, 0xFF, 0xFF);

    error_t IPv4Layer::parse(Mem::SmartPointer<Dev::Net> net_dev, const void* packet_ptr,
                             size_t len) {
        if (len < sizeof(ipv4_header))
            return error_t::EINVAL;

        auto header = (ipv4_header*) packet_ptr;

        if (header->destination != net_dev->ipv4_addr &&
            header->destination != net_dev->ipv4_broadcast && header->destination != BROADCAST_ADDR)
            return error_t::EINVAL;

        printk("ipv4: id %i 0x%x\n", (uint16_t) header->identification,
               (uint16_t) header->identification);

        return error_t::ENONE;
    }
}