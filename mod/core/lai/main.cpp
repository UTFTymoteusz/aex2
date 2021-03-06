#include "aex/arch/sys/cpu.hpp"
#include "aex/kpanic.hpp"
#include "aex/mem.hpp"
#include "aex/module.hpp"
#include "aex/printk.hpp"
#include "aex/proc.hpp"
#include "aex/string.hpp"
#include "aex/sys/acpi.hpp"
#include "aex/sys/pci.hpp"
#include "aex/sys/power.hpp"

#include "lai/core.h"
#include "lai/helpers/pci.h"
#include "lai/helpers/pm.h"
#include "lai/helpers/sci.h"
#include "lai/host.h"

#include <stddef.h>
#include <stdint.h>

using namespace AEX;
using namespace AEX::Mem;
using namespace AEX::Sys;

const char* MODULE_NAME = "lai";

uint8_t acpi_set_pci_pin(uint8_t bus, uint8_t device, uint8_t function, uint8_t pin);
error_t acpi_poweroff();

void module_enter() {
    printk("lai: ACPI revision 0x%02x\n", ACPI::revision);

    lai_set_acpi_revision(ACPI::revision);
    lai_create_namespace();

    lai_enable_acpi(1);

    Sys::Power::register_poweroff_handler(1, acpi_poweroff);

    register_dynamic_symbol("acpi_set_pci_pin", (void*) acpi_set_pci_pin);
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
    return Mem::kernel_pagemap->map(count, addr, PAGE_WRITE);
}

extern "C" void laihost_unmap(void* addr, size_t count) {
    Mem::kernel_pagemap->free(addr, count);
}


extern "C" void* laihost_scan(const char* sig, size_t index) {
    return ACPI::find_table(sig, index);
}


extern "C" void laihost_outb(uint16_t port, uint8_t val) {
    Sys::CPU::outb(port, val);
}

extern "C" void laihost_outw(uint16_t port, uint16_t val) {
    Sys::CPU::outw(port, val);
}

extern "C" void laihost_outd(uint16_t port, uint32_t val) {
    Sys::CPU::outd(port, val);
}

extern "C" uint8_t laihost_inb(uint16_t port) {
    return Sys::CPU::inb(port);
}

extern "C" uint16_t laihost_inw(uint16_t port) {
    return Sys::CPU::inw(port);
}

extern "C" uint32_t laihost_ind(uint16_t port) {
    return Sys::CPU::ind(port);
}


extern "C" void laihost_pci_writeb(uint16_t, uint8_t bus, uint8_t slot, uint8_t fun,
                                   uint16_t offset, uint8_t val) {
    PCI::write_byte(bus, slot, fun, offset, val);
}

extern "C" void laihost_pci_writew(uint16_t, uint8_t bus, uint8_t slot, uint8_t fun,
                                   uint16_t offset, uint16_t val) {
    PCI::write_word(bus, slot, fun, offset, val);
}

extern "C" void laihost_pci_writed(uint16_t, uint8_t bus, uint8_t slot, uint8_t fun,
                                   uint16_t offset, uint32_t val) {
    PCI::write_dword(bus, slot, fun, offset, val);
}

extern "C" uint8_t laihost_pci_readb(uint16_t, uint8_t bus, uint8_t slot, uint8_t fun,
                                     uint16_t offset) {
    return PCI::read_byte(bus, slot, fun, offset);
}

extern "C" uint16_t laihost_pci_readw(uint16_t, uint8_t bus, uint8_t slot, uint8_t fun,
                                      uint16_t offset) {
    return PCI::read_word(bus, slot, fun, offset);
}

extern "C" uint32_t laihost_pci_readd(uint16_t, uint8_t bus, uint8_t slot, uint8_t fun,
                                      uint16_t offset) {
    return PCI::read_dword(bus, slot, fun, offset);
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

error_t acpi_poweroff() {
    lai_enter_sleep(5);
    return ESHUTDOWN;
}