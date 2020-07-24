#include "aex/printk.hpp"

#include "iso9660.hpp"

using namespace AEX;

const char* MODULE_NAME = "iso9660";

void module_enter() {
    FS::ISO9660::init();
}

void module_exit() {
    //
}