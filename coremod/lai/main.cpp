#include "aex/acpi.hpp"
#include "aex/arch/sys/cpu.hpp"
#include "aex/dev/pci.hpp"
#include "aex/kpanic.hpp"
#include "aex/mem/heap.hpp"
#include "aex/mem/vmem.hpp"
#include "aex/module.hpp"
#include "aex/printk.hpp"
#include "aex/proc/thread.hpp"
#include "aex/string.hpp"

#include "lai/core.h"
#include "lai/helpers/pci.h"
#include "lai/helpers/pm.h"
#include "lai/helpers/sci.h"
#include "lai/host.h"

#include <stddef.h>
#include <stdint.h>

using namespace AEX;

const char* MODULE_NAME = "lai";

uint8_t acpi_set_pci_pin(uint8_t bus, uint8_t device, uint8_t function, uint8_t pin);

void module_enter() {
    printk("lai: ACPI revision 0x%02x\n", ACPI::revision);

    lai_set_acpi_revision(ACPI::revision);
    lai_create_namespace();

    lai_enable_acpi(1);

    register_global_symbol("acpi_set_pci_pin", (void*) acpi_set_pci_pin);
}

void module_exit() {}

extern "C" void* memset(void* mem, int c, size_t len) {
    AEX::memset(mem, c, len);
    return mem;
}

extern "C" void* memcpy(void* dst, const void* src, size_t len) {
    AEX::memcpy(dst, src, len);
    return dst;
}

extern "C" int memcmp(const void* a, const void* b, size_t len) {
    return AEX::memcmp(a, b, len);
}


extern "C" void laihost_log(int, const char* msg) {
    printk("lai: %c%s\n", toupper(msg[0]), msg + 1);
}

extern "C" void laihost_panic(const char* msg) {
    kpanic("lai: %s", msg);
}


extern "C" void* laihost_malloc(size_t size) {
    return Heap::malloc(size);
}

extern "C" void* laihost_realloc(void* ptr, size_t size) {
    return Heap::realloc(ptr, size);
}

extern "C" void laihost_free(void* ptr) {
    Heap::free(ptr);
}

extern "C" void* laihost_map(size_t addr, size_t count) {
    return VMem::kernel_pagemap->map(count, addr, PAGE_WRITE);
}

extern "C" void laihost_unmap(void* addr, size_t count) {
    VMem::kernel_pagemap->free(addr, count);
}


extern "C" void* laihost_scan(const char* sig, size_t index) {
    return ACPI::find_table(sig, index);
}


extern "C" void laihost_outb(uint16_t port, uint8_t val) {
    Sys::CPU::outportb(port, val);
}

extern "C" void laihost_outw(uint16_t port, uint16_t val) {
    Sys::CPU::outportw(port, val);
}

extern "C" void laihost_outd(uint16_t port, uint32_t val) {
    Sys::CPU::outportd(port, val);
}

extern "C" uint8_t laihost_inb(uint16_t port) {
    return Sys::CPU::inportb(port);
}

extern "C" uint16_t laihost_inw(uint16_t port) {
    return Sys::CPU::inportw(port);
}

extern "C" uint32_t laihost_ind(uint16_t port) {
    return Sys::CPU::inportd(port);
}


extern "C" void laihost_pci_writeb(uint16_t, uint8_t bus, uint8_t slot, uint8_t fun,
                                   uint16_t offset, uint8_t val) {
    Dev::PCI::write_byte(bus, slot, fun, offset, val);
}

extern "C" void laihost_pci_writew(uint16_t, uint8_t bus, uint8_t slot, uint8_t fun,
                                   uint16_t offset, uint16_t val) {
    Dev::PCI::write_word(bus, slot, fun, offset, val);
}

extern "C" void laihost_pci_writed(uint16_t, uint8_t bus, uint8_t slot, uint8_t fun,
                                   uint16_t offset, uint32_t val) {
    Dev::PCI::write_dword(bus, slot, fun, offset, val);
}

extern "C" uint8_t laihost_pci_readb(uint16_t, uint8_t bus, uint8_t slot, uint8_t fun,
                                     uint16_t offset) {
    return Dev::PCI::read_byte(bus, slot, fun, offset);
}

extern "C" uint16_t laihost_pci_readw(uint16_t, uint8_t bus, uint8_t slot, uint8_t fun,
                                      uint16_t offset) {
    return Dev::PCI::read_word(bus, slot, fun, offset);
}

extern "C" uint32_t laihost_pci_readd(uint16_t, uint8_t bus, uint8_t slot, uint8_t fun,
                                      uint16_t offset) {
    return Dev::PCI::read_dword(bus, slot, fun, offset);
}


extern "C" void laihost_handle_amldebug(lai_variable_t*) {}

extern "C" void laihost_sleep(uint64_t ms) {
    Proc::Thread::sleep(ms);
}


uint8_t acpi_set_pci_pin(uint8_t bus, uint8_t device, uint8_t function, uint8_t pin) {
    acpi_resource_t aaa;
    lai_pci_route_pin(&aaa, 0, bus, device, function, pin);

    return aaa.base;
}