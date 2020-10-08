#pragma once

#include "aex/proc/exec.hpp"

class Elf64Executor : public AEX::Proc::Executor {
    public:
    AEX::error_t exec(const char* path, AEX::Proc::Process* process);
};