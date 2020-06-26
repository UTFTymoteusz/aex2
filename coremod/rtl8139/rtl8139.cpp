#include "aex/arch/sys/cpu.hpp"
#include "aex/debug.hpp"
#include "aex/dev/name.hpp"
#include "aex/dev/netdevice.hpp"
#include "aex/dev/pci.hpp"
#include "aex/dev/tree/tree.hpp"
#include "aex/math.hpp"
#include "aex/mem/vmem.hpp"
#include "aex/net/ipv4.hpp"
#include "aex/net/net.hpp"
#include "aex/printk.hpp"
#include "aex/proc/thread.hpp"
#include "aex/sys/irq.hpp"

#include <stddef.h>
#include <stdint.h>

using namespace AEX;
using namespace AEX::Dev;

using CPU = AEX::Sys::CPU;

const char* MODULE_NAME = "rtl8139";

auto constexpr TSD0     = 0x10;
auto constexpr TSD1     = 0x14;
auto constexpr TSD2     = 0x18;
auto constexpr TSD3     = 0x1C;
auto constexpr TSAD0    = 0x20;
auto constexpr TSAD1    = 0x24;
auto constexpr TSAD2    = 0x28;
auto constexpr TSAD3    = 0x2C;
auto constexpr RB_START = 0x30;
auto constexpr CMD      = 0x37;
auto constexpr CAPR     = 0x38;
auto constexpr CBR      = 0x3A;
auto constexpr IMR      = 0x3C;
auto constexpr ISR      = 0x3E;
auto constexpr TCR      = 0x40;
auto constexpr RCR      = 0x44;
auto constexpr CONFIG_1 = 0x52;

auto constexpr CMD_RST = 0x01 << 4;
auto constexpr CMD_RE  = 0x01 << 3;
auto constexpr CMD_TE  = 0x01 << 2;

auto constexpr IMR_FOV = 0x01 << 6;
auto constexpr IMR_ROV = 0x01 << 4;
auto constexpr IMR_TOK = 0x01 << 2;
auto constexpr IMR_ROK = 0x01 << 0;

auto constexpr ISR_FOV = IMR_FOV;
auto constexpr ISR_ROV = IMR_ROV;
auto constexpr ISR_TOK = IMR_TOK;
auto constexpr ISR_ROK = IMR_ROK;

auto constexpr TSD_CRC        = 0x01 << 16;
auto constexpr TSD_TOK        = 0x01 << 15;
auto constexpr TSD_OWN        = 0x01 << 13;
auto constexpr TSD_MXDMA_1024 = 0b110 << 8;

auto constexpr RCR_SERR = 0x01 << 15;
auto constexpr RCR_B64K = 0x03 << 11;
auto constexpr RCR_B32K = 0x02 << 11;
auto constexpr RCR_B16K = 0x01 << 11;
auto constexpr RCR_B8K  = 0x00 << 11;
auto constexpr RCR_WRAP = 0x01 << 7;
auto constexpr RCR_AR   = 0x01 << 4;
auto constexpr RCR_AB   = 0x01 << 3;
auto constexpr RCR_AM   = 0x01 << 2;
auto constexpr RCR_APM  = 0x01 << 1;
auto constexpr RCR_AAP  = 0x01 << 0;

auto constexpr BUFFER_SIZE = 32768 + 0x10;
auto constexpr BUFFER_MASK = (BUFFER_SIZE - 0x10) - 4;

class RTL8139 : public Dev::NetDevice {
    public:
    RTL8139(PCI::PCIDevice* device, const char* name)
        : NetDevice(name, Net::link_type_t::LINK_ETHERNET) {
        _tx_buffers = (uint8_t*) VMem::kernel_pagemap->allocContinuous(2048 * 4);
        _rx_buffer  = (uint8_t*) VMem::kernel_pagemap->allocContinuous(BUFFER_SIZE + 1500);

        for (int i = 5; i >= 0; i--) {
            auto resource = device->getResource(i);
            if (!resource.has_value)
                continue;

            if (resource.value.type == Tree::Device::resource::type_t::IO)
                _io_base = resource.value.start;
        }

        Net::mac_addr mac;

        for (int i = 0; i < 6; i++)
            mac[i] = CPU::inportb(_io_base + i);

        _irq = device->getIRQ();

        printk("rtl8139: %s: irq %i\n", name, _irq);
        printk("rtl8139: %s: mac %02X:%02X:%02X:%02X:%02X:%02X\n", name, mac[0], mac[1], mac[2],
               mac[3], mac[4], mac[5]);

        info.ipv4.mac = mac;

        // Power on
        CPU::outportb(_io_base + CONFIG_1, 0x00);

        // Software reset
        CPU::outportb(_io_base + CMD, CMD_RST);

        while (CPU::inportb(_io_base + CMD) & CMD_RST)
            Proc::Thread::yield();

        uint32_t tx_paddr = VMem::kernel_pagemap->paddrof(_tx_buffers);
        if (tx_paddr > 0xFFFFFFFF)
            kpanic("rtl8139: TX buffer address > 4gb");

        uint32_t rx_paddr = VMem::kernel_pagemap->paddrof(_rx_buffer);
        if (rx_paddr > 0xFFFFFFFF)
            kpanic("rtl8139: RX buffer address > 4gb");

        // Set transmit buffers addresses
        for (int i = 0; i < 4; i++)
            CPU::outportd(_io_base + TSAD0 + 4 * i, tx_paddr + 2048 * i);

        // Set receive buffer address
        CPU::outportd(_io_base + RB_START, rx_paddr);

        // Enable transmitting and receiving
        CPU::outportb(_io_base + CMD, CMD_TE | CMD_RE);

        // Let's set the params
        CPU::outportd(_io_base + TCR, TSD_CRC | TSD_MXDMA_1024);
        CPU::outportd(_io_base + RCR, RCR_B32K | RCR_WRAP | RCR_AAP | RCR_AB | RCR_AM | RCR_AR);

        Sys::IRQ::register_handler(
            _irq, [](void* dev) { ((RTL8139*) dev)->handleIRQ(); }, this);

        // IMR time
        CPU::outportw(_io_base + IMR, IMR_ROK | IMR_ROV | IMR_FOV);

        printk(PRINTK_OK "rtl8139: %s: Ready\n", name);
    }

    error_t send(const void* buffer, size_t len) {
        if (len < 16)
            return error_t::EINVAL; // change this later pls

        auto scopeLock = ScopeSpinlock(_tx_lock);

        len = min<size_t>(len, 1792);

        uint16_t io_port = _io_base + TSD0 + 4 * _current_tx_buffer;

        while (!(CPU::inportd(io_port) & TSD_OWN))
            ;

        memcpy(_tx_buffers + 2048 * _current_tx_buffer, buffer, len);

        CPU::outportd(io_port, len);

        _current_tx_buffer++;
        if (_current_tx_buffer == 4)
            _current_tx_buffer = 0;

        return error_t::ENONE;
    }

    private:
    struct rx_frame {
        uint16_t flags;
        uint16_t len;
    } __attribute((packed));

    uint32_t _io_base;

    uint8_t* _tx_buffers = nullptr;
    uint8_t* _rx_buffer  = nullptr;

    size_t _rx_buffer_pos = 0;

    uint8_t _irq;

    uint8_t _current_tx_buffer = 0;

    Spinlock _tx_lock;

    void handleIRQ() {
        static Spinlock lock;

        lock.acquire();

        uint16_t status = CPU::inportw(_io_base + ISR);
        uint16_t ack    = 0;

        if (status & ISR_ROK) {
            ack |= ISR_ROK;

            packetReceived();
        }

        if (status & ISR_FOV) {
            ack |= ISR_FOV;
            kpanic("rtl8139: FIFO overflow\n");
        }

        if (status & ISR_ROV) {
            ack |= ISR_ROK;
            kpanic("rtl8139: RX buffer overflow\n");
        }

        CPU::outportw(_io_base + ISR, ack);

        lock.release();
    }

    void packetReceived() {
        while (_rx_buffer_pos != CPU::inportw(_io_base + CBR)) {
            auto frame = (rx_frame*) (&_rx_buffer[_rx_buffer_pos]);
            if (!frame->flags & 0x01)
                break;

            uint16_t frame_len = frame->len;

            receive(&_rx_buffer[_rx_buffer_pos + 4], frame_len);

            _rx_buffer_pos = (_rx_buffer_pos + (frame_len + 4 + 3)) & BUFFER_MASK;

            CPU::outportw(_io_base + CAPR, _rx_buffer_pos - 0x10);
        }
    }
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

        return true;
    }

    void bind(Tree::Device* device) {
        auto pci_device = (PCI::PCIDevice*) device;

        PCI::set_busmaster(pci_device, true);

        char buffer[32];
        Dev::name_number_increment(buffer, sizeof(buffer), "eth%");

        auto rtl            = new RTL8139(pci_device, buffer);
        device->driver_data = rtl;

        if (!rtl->registerDevice())
            printk(PRINTK_WARN "rtl8139: %s: Failed to register\n", rtl->name);

        rtl->setIPv4Address(Net::ipv4_addr(192, 168, 0, 23));
        rtl->setIPv4Mask(Net::ipv4_addr(255, 255, 255, 0));
        rtl->setIPv4Gateway(Net::ipv4_addr(192, 168, 0, 1));
        rtl->setMetric(10000);
    }

    private:
};

RTL8139Driver* driver = nullptr;

void module_enter() {
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