#include "aex/mem/heap.hpp"
#include "aex/printk.hpp"
#include "aex/proc/thread.hpp"

using namespace AEX;

const char* MODULE_NAME = "testmod";

void annoying_a() {
    printk("i literally have no idea what to do so i'll just exit\n");
    Proc::Thread::sleep(2500);
}

void module_enter() {
    auto thread = new Proc::Thread(nullptr, (void*) annoying_a, VMem::kernel_pagemap->alloc(8192),
                                   8192, VMem::kernel_pagemap);

    thread->start();

    printk(PRINTK_WARN "testmod: Loaded\n");
}

void module_exit() {
    printk(PRINTK_WARN "testmod: Exiting\n");
}