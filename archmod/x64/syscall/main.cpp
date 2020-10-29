#include "aex/arch/sys/cpu.hpp"
#include "aex/assert.hpp"
#include "aex/printk.hpp"
#include "aex/proc/thread.hpp"

#include <stdint.h>

using namespace AEX;

constexpr auto IA32_STAR  = 0xC0000081;
constexpr auto IA32_LSTAR = 0xC0000082;

const char* MODULE_NAME = "syscall";

extern "C" void* handler(uint64_t id);
void             register_syscalls();
void             install_handler();

void register_fs();
void register_proc();
void register_test();

void module_enter() {
    Sys::CPU::broadcast(Sys::CPU::IPP_CALL, (void*) install_handler, false);
    register_syscalls();
}

void module_exit() {
    //
}

void install_handler() {
    Sys::CPU::wrmsr(IA32_STAR, (Sys::CPU::rdmsr(IA32_STAR) & 0xFFFFFFFF) | (0x00100008ul << 32));
    Sys::CPU::wrmsr(IA32_LSTAR, (uint64_t) handler);
}

void register_syscalls() {
    register_fs();
    register_proc();
    register_test();
}

extern "C" void syscall_prepare() {
    Proc::Thread::current()->addBusy();
}

extern "C" void syscall_done() {
    Proc::Thread::current()->subBusy();
}