#include "aex/assert.hpp"
#include "aex/proc/exec.hpp"

#include "elf64executor.hpp"

using namespace AEX;

const char* MODULE_NAME = "elf64exec";

Proc::Executor* executor;

void module_enter() {
    executor = new Elf64Executor();
    Proc::registerExecutor(executor);
}

void module_exit() {
    //
}