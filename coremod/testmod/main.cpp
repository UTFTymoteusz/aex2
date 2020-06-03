#include "aex/fs/file.hpp"
#include "aex/mem/heap.hpp"
#include "aex/printk.hpp"
#include "aex/proc/proc.hpp"
#include "aex/proc/thread.hpp"
#include "aex/tty.hpp"


// clang-format off
#include "aex/arch/sys/cpu.hpp"
// clang-format on

using namespace AEX;

const char* MODULE_NAME = "testmod";

void aaa();
void bbb() {
    printk("function before works\n");
}

void annoying_a() {
    // printk("i literally have no idea what to do so i'll just exit\n");
    Proc::Thread::sleep(2500);
}

void module_enter() {
    auto thread = new Proc::Thread(nullptr, (void*) annoying_a, VMem::kernel_pagemap->alloc(8192),
                                   8192, VMem::kernel_pagemap);

    thread->start();

    aaa();
    bbb();

    printk(PRINTK_WARN "testmod: Loaded\n");
}

void module_exit() {
    printk(PRINTK_WARN "testmod: Exiting\n");
}

void aaa() {
    printk("function after works\n");
}