#include "aex/mem/heap.hpp"
#include "aex/printk.hpp"
#include "aex/proc/thread.hpp"

using namespace AEX;

volatile int bong = 2;

const char* MODULE_NAME = "testmod";

void annoying_a() {
    while (true) {
        printk("amma keep annoying you\n");
        Proc::Thread::sleep(2500);
    }
}

void module_enter() {
    bong++;

    auto thread = new Proc::Thread(nullptr, (void*) annoying_a, VMem::kernel_pagemap->alloc(8192),
                                   8192, VMem::kernel_pagemap);

    thread->start();

    printk(PRINTK_WARN "testmod: Loaded\n");
    printk(PRINTK_WARN "testmod: Loaded 2\n");
}

void module_exit() {
    printk(PRINTK_WARN "testmod: Exiting\n");
}