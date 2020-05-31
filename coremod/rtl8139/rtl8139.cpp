#include "aex/arch/sys/cpu.hpp"
#include "aex/dev/pci.hpp"
#include "aex/dev/tree/tree.hpp"
#include "aex/printk.hpp"

using namespace AEX;
using namespace AEX::Dev;

const char* MODULE_NAME = "rtl8139";

class RTL8139 {
  public:
    RTL8139(Tree::Device* device) {
        for (int i = 5; i >= 0; i--) {
            auto resource = device->getResource(i);
            if (!resource.has_value)
                continue;

            if (resource.value.type == Tree::Device::resource::type_t::IO)
                _io_base = resource.value.start;
        }

        for (int i = 0; i < 6; i++)
            _mac[i] = Sys::CPU::inportb(_io_base + i);

        printk("rtl8139: MAC %02X:%02X:%02X:%02X:%02X:%02X\n", _mac[0], _mac[1], _mac[2], _mac[3],
               _mac[4], _mac[5]);
    }

  private:
    uint8_t _mac[6];

    uint32_t _io_base;
};

class RTL8139Driver : public Tree::Driver {
  public:
    RTL8139Driver() : Driver("rtl8139") {}
    ~RTL8139Driver() {}

    bool check(Tree::Device* device) {
        auto pci_device = (PCI::PCIDevice*) device;

        if (pci_device->p_class != 0x02 || pci_device->subclass != 0x00 ||
            pci_device->vendor_id != 0x10EC || pci_device->device_id != 0x8139)
            return false;

        printk("found a rtl8139\n");

        return true;
    }

    void bind(Tree::Device* device) {
        PCI::set_busmaster(device, true);

        auto rtl            = new RTL8139(device);
        device->driver_data = rtl;
    }

  private:
};

RTL8139Driver* driver = nullptr;

void module_enter() {
    printk("rtl8139: Module loaded\n");

    driver = new RTL8139Driver();

    if (!Tree::register_driver("pci", driver)) {
        printk(PRINTK_WARN "rtl8139: Failed to register the driver\n");

        delete driver;
        return;
    }
}

void module_exit() {
    // Gotta definitely work on this
}