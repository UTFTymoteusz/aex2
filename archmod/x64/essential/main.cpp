#include "aex/assert.hpp"
#include "aex/proc/exec.hpp"

using namespace AEX;

const char* MODULE_NAME = "essential";

void power_init();

void module_enter() {
    power_init();
}

void module_exit() {
    //
}