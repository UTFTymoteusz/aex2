#include "aex/printk.hpp"

using namespace AEX;

const char* MODULE_NAME = "sata";

namespace AEX::Sys::SATA {
    void init();
}

void module_enter() {
    Sys::SATA::init();
}

void module_exit() {
    //
}