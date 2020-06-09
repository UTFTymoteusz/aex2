#include "aex/net/linklayer.hpp"
#include "aex/net/net.hpp"
#include "aex/net/socket.hpp"
#include "aex/printk.hpp"

#include "layer/ethernet.hpp"
#include "layer/none.hpp"
#include "loopbackdev.hpp"
#include "protocol/udp.hpp"
#include "rx_core.hpp"
#include "tx_core.hpp"

// clang-format off
#include "layer/arp.hpp"
#include "aex/proc/thread.hpp"
// clang-format on

using namespace AEX;

const char* MODULE_NAME = "netstack";

void module_enter() {
    NetStack::tx_init();
    NetStack::rx_init();

    NetStack::ARPLayer::add_static_entry(Net::ipv4_addr(255, 255, 255, 255),
                                         Net::mac_addr(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF));

    NetStack::UDPProtocol::init();

    auto loopback_dev = new NetStack::Loopback();
    if (!loopback_dev->registerDevice())
        kpanic("netstack: Failed to register the loopback device");

    loopback_dev->setIPv4Address(Net::ipv4_addr(127, 0, 0, 1));
    loopback_dev->setIPv4Mask(Net::ipv4_addr(255, 0, 0, 0));
    loopback_dev->setMetric(1000000);

    Net::register_link_layer(Net::link_type_t::LINK_NONE, new NetStack::NoneLayer());
    Net::register_link_layer(Net::link_type_t::LINK_ETHERNET, new NetStack::EthernetLayer());
    Net::register_inet_protocol(socket_protocol_t::IPROTO_UDP, new NetStack::UDPProtocol());

    /*while (true) {
        Proc::Thread::sleep(1000);
        auto mac_try = NetStack::ARPLayer::query_ipv4(
            NetStack::get_interface_by_srcaddr(ipv4_addr(192, 168, 0, 23)), ipv4_addr(192, 168, 0,
    220));

        if (!mac_try.has_value) {
            printk("arp: Failed\n");
            continue;
        }

        auto mac = mac_try.value;

        printk("arp: Success: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3],
               mac[4], mac[5]);
    }*/
}

void module_exit() {}