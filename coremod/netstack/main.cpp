#include "aex/assert.hpp"
#include "aex/net.hpp"
#include "aex/printk.hpp"

#include "layer/link/arp.hpp"
#include "layer/network/ipv4.hpp"
#include "layer/transport/tcp.hpp"
#include "loopbackdev.hpp"
#include "protocol/inet/tcp.hpp"
#include "protocol/inet/udp.hpp"

using namespace AEX;
using namespace AEX::Net;
using namespace NetStack;

const char* MODULE_NAME = "netstack";

void module_enter() {
    ARPLayer::init();
    IPv4Layer::init();
    TCPLayer::init();

    auto loopback_dev = new Loopback();
    AEX_ASSERT(loopback_dev->registerDevice());

    loopback_dev->setIPv4Address(Net::ipv4_addr(127, 0, 0, 1));
    loopback_dev->setIPv4Mask(Net::ipv4_addr(255, 0, 0, 0));
    loopback_dev->setMetric(1000000);

    UDPProtocol::init();
    TCPProtocol::init();

    register_inet_protocol(IPROTO_TCP, new TCPProtocol());
    register_inet_protocol(IPROTO_UDP, new UDPProtocol());
}

void module_exit() {}