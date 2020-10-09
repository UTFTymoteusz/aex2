#include "aex/errno.hpp"
#include "aex/printk.hpp"
#include "aex/sys/syscall.hpp"

#include "syscallids.h"

#include <stdint.h>

using namespace AEX;

error_t test1(uint64_t a) {
    printk("syscall: test1(0x%x)\n", a);
    return ENONE;
}

error_t test2(uint64_t a, uint64_t b) {
    printk("syscall: test2(0x%x, 0x%x)\n", a, b);
    return ENONE;
}

error_t test3(uint64_t a, uint64_t b, uint64_t c) {
    printk("syscall: test3(0x%x, 0x%x, 0x%x)\n", a, b, c);
    return ENONE;
}

error_t test4(uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
    printk("syscall: test4(0x%x, 0x%x, 0x%x, 0x%x)\n", a, b, c, d);
    return ENONE;
}

error_t test5(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e) {
    printk("syscall: test5(0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n", a, b, c, d, e);
    return ENONE;
}

void register_test() {
    auto table = Sys::default_table();

    table[SYS_TEST1] = (void*) test1;
    table[SYS_TEST2] = (void*) test2;
    table[SYS_TEST3] = (void*) test3;
    table[SYS_TEST4] = (void*) test4;
    table[SYS_TEST5] = (void*) test5;
}