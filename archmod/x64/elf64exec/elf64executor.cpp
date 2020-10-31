#include "elf64executor.hpp"

#include "aex/elf.hpp"
#include "aex/fs.hpp"

#include <stddef.h>
#include <stdint.h>

using namespace AEX;

error_t Elf64Executor::exec(const char* path, Proc::Process* process) {
    printk("elf64exec: Got a request for %s\n", path);

    auto file_try = FS::File::open(path, FS::O_RD);
    if (!file_try)
        return file_try.error_code;

    auto    file = file_try.value;
    int64_t size = file->seek(0, FS::File::SEEK_END).value;

    auto mmap_try = Mem::mmap(nullptr, size, Mem::PROT_READ, Mem::MAP_NONE, file, 0);
    file->close();

    if (!mmap_try)
        return mmap_try;

    void* addr = mmap_try.value;
    auto  elf  = ELF(addr);

    if (!elf.isValid(ELF::BIT_64, ELF::EN_LITTLE, ELF::ISA_AMD64))
        return ENOEXEC;

    for (int i = 0; i < elf.section_headers.count(); i++) {
        auto section_header = elf.section_headers[i];

        if (!(section_header.flags & ELF::SC_ALLOC) || section_header.size == 0)
            continue;

        size_t start = (size_t) section_header.address;
        size_t end   = start + section_header.size;

        size_t fptr = section_header.file_offset;

        for (size_t ptr = start; ptr < end;) {
            size_t chk_size = min(Sys::CPU::PAGE_SIZE - (ptr & 0x0FFF), end - ptr);

            Mem::phys_addr paddr = process->pagemap->paddrof((void*) ptr);
            if (!paddr) {
                void* vaddr = process->pagemap->alloc(
                    chk_size, PAGE_USER | PAGE_WRITE | PAGE_EXEC | PAGE_FIXED, (void*) ptr);
                paddr = process->pagemap->paddrof(vaddr);
            }

            if (section_header.type != ELF::sc_type_t::SC_NO_DATA) {
                void* kaddr = Mem::kernel_pagemap->map(chk_size, paddr, PAGE_WRITE);
                memcpy(kaddr, (uint8_t*) addr + fptr, chk_size);
                Mem::kernel_pagemap->free(kaddr, chk_size);
            }

            ptr += chk_size;
            fptr += chk_size;
        }
    }

    Mem::munmap(addr, size);

    auto thread_try = Proc::Thread::create(process->pid, elf.entry, Proc::Thread::USER_STACK_SIZE,
                                           process->pagemap, true);
    if (!thread_try)
        return EBOTHER;

    return ENONE;
}