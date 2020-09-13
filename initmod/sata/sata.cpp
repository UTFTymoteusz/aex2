#include "aex/dev/tree.hpp"
#include "aex/mem.hpp"
#include "aex/printk.hpp"
#include "aex/sys/pci.hpp"

#include "ahci.hpp"

#include <stddef.h>
#include <stdint.h>

using namespace AEX::Dev;
using namespace AEX::Sys::PCI;

namespace AEX::Sys::SATA {
    extern void sd_init();
    extern void sr_init();

    Tree::Bus* sata_bus;

    class SATA : public Tree::Driver {
        public:
        SATA() : Driver("sata") {}
        ~SATA() {}

        bool check(Tree::Device* device) {
            auto pci_device = (PCIDevice*) device;

            if (pci_device->p_class != 0x01 || pci_device->subclass != 0x06 ||
                pci_device->prog_if != 0x01)
                return false;

            return true;
        }

        void bind(Tree::Device* device) {
            init();
            set_busmaster(device, true);

            Mem::Phys::phys_addr paddr = 0;

            for (int i = 5; i >= 0; i--) {
                auto resource = device->getResource(i);
                if (!resource || resource.value.type != Tree::resource::MEMORY)
                    continue;

                paddr = resource.value.start;
                break;
            }

            void* addr = Mem::kernel_pagemap->map(0x1000, paddr, PAGE_NOCACHE | PAGE_WRITE);
            auto  ahci = new AHCI(device, addr, index);

            index++;

            device->driver_data = ahci;
        }

        private:
        int index = 0;

        void init() {
            if (Tree::bus_exists("sata"))
                return;

            sata_bus = new Tree::Bus("sata");

            sd_init();
            sr_init();
        }
    };

    void init() {
        auto sata = new SATA();

        if (!Tree::register_driver("pci", sata)) {
            printk(PRINTK_WARN "sata: Failed to register the PCI driver\n");

            delete sata;
            return;
        }
    }
}