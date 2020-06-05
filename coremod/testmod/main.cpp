#include "aex/fs/file.hpp"
#include "aex/mem/heap.hpp"
#include "aex/printk.hpp"
#include "aex/proc/proc.hpp"
#include "aex/proc/thread.hpp"
#include "aex/tty.hpp"

using namespace AEX;

const char* MODULE_NAME = "testmod";

void module_enter() {
    printk(PRINTK_WARN "testmod: Loaded\n");
}

void module_exit() {
    printk(PRINTK_WARN "testmod: Exiting\n");
}