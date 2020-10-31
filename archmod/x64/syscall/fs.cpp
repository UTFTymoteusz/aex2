#include "aex/errno.hpp"
#include "aex/fs/file.hpp"
#include "aex/proc/process.hpp"
#include "aex/sys/syscall.hpp"

#include "syscallids.h"

using namespace AEX;

int open(const char* usr_path, int mode) {
    auto current = Proc::Process::current();
    auto file    = FS::File::open(usr_path, mode);

    return current->files.push(file);
}

uint32_t read(int fd, void* usr_buf, uint32_t count) {
    auto current = Proc::Process::current();

    if (!current->files.present(fd)) {
        PRINTK_DEBUG_WARN1("ebadf (%i)\n", fd);
        return EBADF;
    }

    auto file     = current->files[fd];
    auto read_try = file->read(usr_buf, count);

    return read_try.value;
}

uint32_t write(int fd, void* usr_buf, uint32_t count) {
    auto current = Proc::Process::current();

    if (!current->files.present(fd)) {
        PRINTK_DEBUG_WARN1("ebadf (%i)\n", fd);
        return EBADF;
    }

    auto file      = current->files[fd];
    auto write_try = file->write(usr_buf, count);

    return write_try.value;
}

void register_fs() {
    auto table = Sys::default_table();

    table[SYS_OPEN]  = (void*) open;
    table[SYS_READ]  = (void*) read;
    table[SYS_WRITE] = (void*) write;
}