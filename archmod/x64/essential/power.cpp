#include "aex/sys/power.hpp"

#include "aex/arch/sys/cpu.hpp"
#include "aex/debug.hpp"
#include "aex/errno.hpp"

using namespace AEX;

error_t triple_fault();

void power_init() {
    Sys::Power::register_poweroff_handler(1000, triple_fault);
}

error_t triple_fault() {
    Sys::CPU::tripleFault();
    return ENONE;
}