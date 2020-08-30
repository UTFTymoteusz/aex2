#include "netstack/arp.hpp"

#include "layer/link/arp.hpp"

namespace NetStack {
    AEX::optional<AEX::Net::mac_addr> arp_get_mac(AEX::Net::ipv4_addr ipv4) {
        return ARPLayer::get_mac(ipv4);
    }
}