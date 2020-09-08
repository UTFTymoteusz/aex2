#include "tcp.hpp"

#include "aex/printk.hpp"
#include "aex/string.hpp"

#include "checksum.hpp"
#include "layer/network/ipv4.hpp"
#include "protocol/inet/tcp.hpp"

namespace NetStack {
    void TCPLayer::init() {
        //
    }

    void TCPLayer::parse(const uint8_t* buffer, uint16_t len, AEX::Net::ipv4_addr src_addr,
                         AEX::Net::ipv4_addr dst_addr) {
        if (len < sizeof(tcp_header))
            return;

        auto header = (tcp_header*) buffer;

        tcp_fake_ipv4_header fake_header;

        fake_header.source      = src_addr;
        fake_header.destination = dst_addr;
        fake_header.zero        = 0;
        fake_header.protocol    = 6;
        fake_header.length      = len;

        uint16_t hdr_len = (uint16_t) header->goddamned_bitvalues >> 10;
        if (hdr_len > len)
            return;

        uint32_t total = sum_bytes(&fake_header, sizeof(fake_header)) + sum_bytes(buffer, len);
        if (to_checksum(total) != 0x0000) {
            AEX::printk(PRINTK_WARN "tcp: Got a packet with an invalid checksum (0x%04x)\n",
                        to_checksum(total));
            return;
        }

        TCPProtocol::packetReceived(src_addr, (uint16_t) header->source_port, dst_addr,
                                    (uint16_t) header->destination_port, buffer, len);
    }
}