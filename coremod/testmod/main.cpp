#include "aex/fs/file.hpp"
#include "aex/mem.hpp"
#include "aex/printk.hpp"
#include "aex/proc.hpp"

using namespace AEX;

const char* MODULE_NAME = "testmod";

void test_mmap();

void module_enter() {
    printk(PRINTK_WARN "testmod: Loaded\n");

    test_mmap();
}

void module_exit() {
    printk(PRINTK_WARN "testmod: Exiting\n");
}