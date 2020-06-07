#include "aex/net/linklayer.hpp"
#include "aex/net/net.hpp"

#include "layer/ethernet.hpp"
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

    Net::register_link_layer(Net::link_type_t::LINK_ETHERNET, new NetStack::EthernetLayer());

    /*while (true) {
        Proc::Thread::sleep(1000);
        auto mac_try = NetStack::ARPLayer::query_ipv4(
            NetStack::get_interface(ipv4_addr(192, 168, 0, 23)), ipv4_addr(192, 168, 0, 220));

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