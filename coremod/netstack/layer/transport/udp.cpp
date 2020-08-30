#include "udp.hpp"

#include "aex/printk.hpp"

#include "checksum.hpp"
#include "protocol/inet/udp.hpp"

namespace NetStack {
    void UDPLayer::parse(const uint8_t* buffer, uint16_t len, AEX::Net::ipv4_addr src_addr,
                         AEX::Net::ipv4_addr dst_addr) {
        if (len < sizeof(udp_header))
            return;

        auto header = (udp_header*) buffer;

        if (header->checksum != 0x0000) {
            udp_fake_ipv4_header fake_header;

            fake_header.source      = src_addr;
            fake_header.destination = dst_addr;
            fake_header.zero        = 0;
            fake_header.protocol    = 17;
            fake_header.length      = header->total_length;

            uint32_t total = sum_bytes(&fake_header, sizeof(fake_header)) +
                             sum_bytes(buffer, header->total_length);
            if (to_checksum(total) != 0x0000) {
                AEX::printk(PRINTK_WARN "udp: Got a packet with an invalid checksum\n");
                return;
            }
        }

        buffer += sizeof(udp_header);
        len -= sizeof(udp_header);

        UDPProtocol::packetReceived(src_addr, (uint16_t) header->source_port, dst_addr,
                                    (uint16_t) header->destination_port, buffer, len);
    }
}
