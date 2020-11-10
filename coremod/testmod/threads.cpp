#include "aex/assert.hpp"
#include "aex/printk.hpp"
#include "aex/proc.hpp"
#include "aex/sys/time.hpp"

using namespace AEX;

namespace AEX::Proc {
    void debug_print_cpu_jobs();
    void debug_print_threads();
}

void boi_a() {
    Proc::Thread::sleep(100);
}

void boi_b() {
    auto thread_a = Proc::threaded_call(boi_a);
    thread_a->join();
}

void test_threads() {
    auto thread_b = Proc::threaded_call(boi_b);
    thread_b->detach();

    Proc::Thread::sleep(50);

    thread_b->abort();
    printk("seems to be fine\n");
}