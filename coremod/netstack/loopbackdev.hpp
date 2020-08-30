#pragma once

#include "aex/dev.hpp"

#include "netstack.hpp"

namespace NetStack {
    class Loopback : public AEX::Dev::NetDevice {
        public:
        Loopback() : AEX::Dev::NetDevice("lo", AEX::Net::LINK_NONE) {}

        AEX::error_t send(const void* buffer, size_t len, AEX::Net::net_type_t type) {
            using namespace AEX;

            receive((const uint8_t*) buffer, len, type);
            return ENONE;
        }

        void receive(const uint8_t* buffer, uint16_t len, AEX::Net::net_type_t type) {
            using namespace AEX;

            switch (type) {
            case Net::NET_ARP:
                arp_received(id, buffer, len);
                break;
            case Net::NET_IPv4:
                ipv4_received(id, buffer, len);
                break;
            case Net::NET_IPv6:
                ipv6_received(id, buffer, len);
                break;
            default:
                break;
            }
        }
    };
}