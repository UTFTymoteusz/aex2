#include "layer/tcp.hpp"

#include "aex/printk.hpp"

#include "checksum.hpp"
#include "protocol/tcp.hpp"

namespace AEX::NetStack {
    void TCPLayer::parse(Dev::NetDevice_SP, Net::ipv4_addr source, Net::ipv4_addr destination,
                         uint8_t* buffer, uint16_t len) {
        if (len < sizeof(tcp_header))
            return;

        auto header = (tcp_header*) buffer;

        tcp_fake_ipv4_header fake_header;

        fake_header.source      = source;
        fake_header.destination = destination;
        fake_header.zero        = 0;
        fake_header.protocol    = 6;
        fake_header.length      = len;

        uint16_t hdr_len = (uint16_t) header->fucking_bitvalues >> 10;
        if (hdr_len > len)
            return;

        uint32_t total = sum_bytes(&fake_header, sizeof(fake_header)) + sum_bytes(buffer, len);
        if (to_checksum(total) != 0x0000) {
            printk(PRINTK_WARN "tcp: Got a packet with an invalid checksum\n");
            return;
        }

        TCPProtocol::packetReceived(source, (uint16_t) header->source_port, destination,
                                    (uint16_t) header->destination_port, buffer, len);
    }
}
