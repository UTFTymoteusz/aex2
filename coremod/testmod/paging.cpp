#include "aex/mem/paging.hpp"

#include "aex/assert.hpp"
#include "aex/printk.hpp"
#include "aex/sys/time.hpp"

using namespace AEX;

void test_paging_one();

void test_paging() {
    Sys::Time::timediff_t avg = 0;
    Sys::Time::timediff_t max = 0;
    Sys::Time::timediff_t min = 11111111110;

    for (int i = 0; i < 200; i++) {
        auto time = Sys::Time::uptime();

        test_paging_one();

        auto diff = Sys::Time::uptime() - time;
        if (diff > max)
            max = diff;

        if (diff < min)
            min = diff;

        avg += diff;
    }

    avg /= 200;

    printk("paging: max: %li ns\n", max);
    printk("paging: avg: %li ns\n", avg);
    printk("paging: min: %li ns\n", min);
}

void test_paging_one() {
    void* addr = Mem::kernel_pagemap->alloc(65536);
    Mem::kernel_pagemap->free(addr, 65536);
}