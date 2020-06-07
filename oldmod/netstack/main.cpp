#include "aex/net/net.hpp"

#include "core/netcore.hpp"
#include "layer/ethernet.hpp"


using namespace AEX;
using namespace AEX::NetProto;

const char* MODULE_NAME = "netstack";

void module_enter() {
    NetCore::init();

    Net::register_link_layer(Net::link_type_t::LINK_ETHERNET, new EthernetLayer());
}

void module_exit() {}