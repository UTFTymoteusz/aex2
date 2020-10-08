#include "aex/assert.hpp"
#include "aex/printk.hpp"
#include "aex/proc.hpp"
#include "aex/sys/time.hpp"

using namespace AEX;

void test_process_thread() {
    Proc::Thread::sleep(2000);
}

void test_processes() {
    /*auto process =
        new Proc::Process("/bin/bigbong", Proc::Process::current()->pid, Mem::kernel_pagemap);
    process->ready();

    auto thread =
        Proc::Thread::create(process->pid, (void*) test_process_thread, 8192, Mem::kernel_pagemap);
    thread.value->detach();

    int  status = 0;
    auto pid    = Proc::Process::wait(status);*/
}