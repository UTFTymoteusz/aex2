#include "aex/dev/tty.hpp"
#include "aex/fs/file.hpp"
#include "aex/mem.hpp"
#include "aex/printk.hpp"
#include "aex/proc.hpp"

using namespace AEX;

const char* MODULE_NAME = "testmod";

void test_mmap();
void test_paging();
void test_threads();
void test_processes();

void module_enter() {
    printk(PRINTK_WARN "testmod: Loaded\n");

    test_mmap();
    test_paging();
    test_threads();
    // test_processes();

    // while (true)
    //    printk("a: %c ", Dev::TTY::VTTYs[Dev::TTY::ROOT_TTY]->read());
}

void module_exit() {
    printk(PRINTK_WARN "testmod: Exiting\n");
}