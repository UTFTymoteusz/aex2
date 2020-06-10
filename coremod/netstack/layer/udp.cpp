#include "layer/udp.hpp"

#include "aex/printk.hpp"

#include "checksum.hpp"
#include "protocol/udp.hpp"

namespace AEX::NetStack {
    void UDPLayer::parse(Dev::NetDevice_SP, Net::ipv4_addr source, Net::ipv4_addr destination,
                         uint8_t* buffer, uint16_t len) {
        if (len < sizeof(udp_header))
            return;

        auto header = (udp_header*) buffer;

        if (header->checksum != 0x0000) {
            udp_fake_ipv4_header fake_header;

            fake_header.source      = source;
            fake_header.destination = destination;
            fake_header.zero        = 0;
            fake_header.protocol    = 17;
            fake_header.length      = header->total_length;

            uint32_t total = sum_bytes(&fake_header, sizeof(fake_header)) +
                             sum_bytes(buffer, header->total_length);
            if (to_checksum(total) != 0x0000) {
                printk(PRINTK_WARN "udp: Got a packet with an invalid checksum\n");
                return;
            }
        }

        buffer += sizeof(udp_header);
        len -= sizeof(udp_header);

        UDPProtocol::packetReceived(source, (uint16_t) header->source_port, destination,
                                    (uint16_t) header->destination_port, buffer, len);
    }
}
