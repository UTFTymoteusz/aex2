#include "aex/printk.hpp"

using namespace AEX;

const char* MODULE_NAME = "sata";

namespace AEX::Dev::SATA {
    void init();
}

void module_enter() {
    Dev::SATA::init();
}

void module_exit() {
    //
}