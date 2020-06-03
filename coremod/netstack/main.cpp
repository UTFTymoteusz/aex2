#include "aex/net/net.hpp"

#include "core/netcore.hpp"
#include "layer/ethernet.hpp"


using namespace AEX;
using namespace AEX::NetProto;

const char* MODULE_NAME = "netstack";

void module_enter() {
    NetCore::init();

    Net::register_link_layer(Net::llayer_type_t::ETHERNET, new EthernetLayer());
}

void module_exit() {}