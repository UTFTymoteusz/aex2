#pragma once

#include "aex/dev/netdevice.hpp"

#include "layer/link/arp/types.hpp"
#include "netstack/arp.hpp"

#include <stdint.h>

namespace NetStack {
    constexpr arp_uuid_t to_arp_uuid(uint16_t hardware, uint16_t protocol) {
        return (hardware) + (protocol << 16);
    }

    class ARPLayer {
        public:
        static void init();

        static void parse(AEX::Dev::NetDevice_SP dev, const uint8_t* buffer, uint16_t len);

        static AEX::optional<AEX::Net::mac_addr> get_mac(AEX::Net::ipv4_addr ipv4);

        private:
        static void handle_eth_ipv4(AEX::Dev::NetDevice_SP dev, arp_packet* packet);
    };
}