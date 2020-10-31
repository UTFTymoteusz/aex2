#include "aex/arch/sys/cpu.hpp"
#include "aex/assert.hpp"
#include "aex/debug.hpp"
#include "aex/dev.hpp"
#include "aex/dev/tree.hpp"
#include "aex/math.hpp"
#include "aex/mem.hpp"
#include "aex/net.hpp"
#include "aex/printk.hpp"
#include "aex/proc.hpp"
#include "aex/sys/irq.hpp"
#include "aex/sys/pci.hpp"

#include "netstack.hpp"
#include "netstack/arp.hpp"
#include "netstack/ethernet.hpp"
#include "netstack/ipv4.hpp"

#include <stddef.h>
#include <stdint.h>

using namespace AEX;
using namespace AEX::Dev;
using namespace AEX::Sys::PCI;

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
    RTL8139(PCIDevice* device, const char* name) : NetDevice(name, Net::LINK_ETHERNET) {
        m_tx_buffers = (uint8_t*) Mem::kernel_pagemap->allocContinuous(2048 * 4);
        m_rx_buffer  = (uint8_t*) Mem::kernel_pagemap->allocContinuous(BUFFER_SIZE + 1500);

        for (int i = 5; i >= 0; i--) {
            auto resource = device->getResource(i);
            if (!resource)
                continue;

            if (resource.value.type == Tree::resource::IO)
                m_io_base = resource.value.start;
        }

        Net::mac_addr mac;

        for (int i = 0; i < 6; i++)
            mac[i] = CPU::inb(m_io_base + i);

        m_irq = device->getIRQ();

        printk("rtl8139: %s: irq %i\n", name, m_irq);
        printk("rtl8139: %s: mac %02X:%02X:%02X:%02X:%02X:%02X\n", name, mac[0], mac[1], mac[2],
               mac[3], mac[4], mac[5]);

        info.ipv4.mac = mac;

        // Power on
        CPU::outb(m_io_base + CONFIG_1, 0x00);

        // Software reset
        CPU::outb(m_io_base + CMD, CMD_RST);

        while (CPU::inb(m_io_base + CMD) & CMD_RST)
            Proc::Thread::yield();

        Mem::phys_addr tx_paddr = Mem::kernel_pagemap->paddrof(m_tx_buffers);
        AEX_ASSERT(tx_paddr <= 0xFFFFFFFF);

        Mem::phys_addr rx_paddr = Mem::kernel_pagemap->paddrof(m_rx_buffer);
        AEX_ASSERT(rx_paddr <= 0xFFFFFFFF);

        // Set transmit buffers addresses
        for (int i = 0; i < 4; i++)
            CPU::outd(m_io_base + TSAD0 + 4 * i, tx_paddr + 2048 * i);

        // Set receive buffer address
        CPU::outd(m_io_base + RB_START, rx_paddr);

        // Enable transmitting and receiving
        CPU::outb(m_io_base + CMD, CMD_TE | CMD_RE);

        // Let's set the params
        CPU::outd(m_io_base + TCR, TSD_CRC | TSD_MXDMA_1024);
        CPU::outd(m_io_base + RCR, RCR_B32K | RCR_WRAP | RCR_AAP | RCR_AB | RCR_AM | RCR_AR);

        Sys::IRQ::register_handler(
            m_irq, [](void* dev) { ((RTL8139*) dev)->handleIRQ(); }, this);

        // IMR time
        CPU::outw(m_io_base + IMR, IMR_ROK | IMR_ROV | IMR_FOV);

        printk(PRINTK_OK "rtl8139: %s: Ready\n", name);
    }

    error_t send(const void* buffer, size_t len, Net::net_type_t type) {
        if (len < 16)
            return EINVAL; // change this later pls

        ScopeSpinlock scopeLock(m_tx_lock);

        len = min<size_t>(len, 1792);

        uint16_t io_port = m_io_base + TSD0 + 4 * m_tx_buffer_current;
        while (!(CPU::ind(io_port) & TSD_OWN)) {
            m_tx_lock.release();
            m_tx_lock.acquire();
        }

        uint8_t* tx_buffer = m_tx_buffers + 2048 * m_tx_buffer_current;

        switch (type) {
        case Net::NET_ARP: {
            auto frame = (NetStack::ethernet_frame*) tx_buffer;
            auto arp   = (NetStack::arp_packet*) buffer;

            frame->destination = arp->header.operation != NetStack::ARP_REQUEST
                                     ? arp->eth_ipv4.destination_mac
                                     : Net::mac_addr(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
            frame->source    = this->info.ipv4.mac;
            frame->ethertype = NetStack::ETH_ARP;

            memcpy(frame->payload, buffer, len);

            len += sizeof(NetStack::ethernet_frame);
        } break;
        case Net::NET_IPv4: {
            auto frame = (NetStack::ethernet_frame*) tx_buffer;
            auto ipv4  = (NetStack::ipv4_header*) buffer;

            if (ipv4->destination == info.ipv4.broadcast ||
                ipv4->destination == NetStack::IPv4_BROADCAST) {
                frame->destination = Net::mac_addr(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
            }
            else {
                auto arp_addr = ipv4->destination.isSubnettedWith(info.ipv4.addr, info.ipv4.mask)
                                    ? ipv4->destination
                                    : info.ipv4.gateway;
                auto mac_try = NetStack::arp_get_mac(arp_addr);
                if (!mac_try)
                    return EAGAIN;

                frame->destination = mac_try.value;
            }

            frame->source    = info.ipv4.mac;
            frame->ethertype = NetStack::ETH_IPv4;

            memcpy(frame->payload, buffer, len);

            len += sizeof(NetStack::ethernet_frame);
        } break;
        default:
            break;
        }

        CPU::outd(io_port, len);

        m_tx_buffer_current++;
        if (m_tx_buffer_current == 4)
            m_tx_buffer_current = 0;

        return ENONE;
    }

    void receive(const void* buffer, size_t len) {
        if (len < sizeof(NetStack::ethernet_frame))
            return;

        auto frame = (NetStack::ethernet_frame*) buffer;

        if (!frame->destination.isBroadcast() && frame->destination != this->info.ipv4.mac)
            return;

        len -= sizeof(NetStack::ethernet_frame);

        switch ((uint16_t) frame->ethertype) {
        case NetStack::ETH_ARP:
            NetStack::arp_received(id, frame->payload, len);
            break;
        case NetStack::ETH_IPv4:
            NetStack::ipv4_received(id, frame->payload, len);
            break;
        case NetStack::ETH_IPv6:
            NetStack::ipv6_received(id, frame->payload, len);
            break;
        default:
            break;
        }
    }

    private:
    struct rx_frame {
        little_endian<uint16_t> flags;
        little_endian<uint16_t> len;
    } __attribute((packed));

    uint32_t m_io_base;

    uint8_t* m_tx_buffers        = nullptr;
    uint8_t  m_tx_buffer_current = 0;
    uint8_t* m_rx_buffer         = nullptr;
    size_t   m_rx_buffer_pos     = 0;

    uint8_t m_irq;

    Spinlock m_tx_lock;

    void handleIRQ() {
        static Spinlock lock;

        lock.acquire();

        uint16_t status = CPU::inw(m_io_base + ISR);
        uint16_t ack    = 0;

        if (status & ISR_ROK) {
            ack |= ISR_ROK;

            packetReceived();
        }

        if (status & ISR_FOV) {
            ack |= ISR_FOV;
            kpanic("rtl8139: FIFO overflow");
        }

        if (status & ISR_ROV) {
            ack |= ISR_ROK;
            kpanic("rtl8139: RX buffer overflow");
        }

        CPU::outw(m_io_base + ISR, ack);

        lock.release();
    }

    void packetReceived() {
        while (m_rx_buffer_pos != CPU::inw(m_io_base + CBR)) {
            auto frame = (rx_frame*) (&m_rx_buffer[m_rx_buffer_pos]);
            if (!(frame->flags & 0x01))
                break;

            uint16_t frame_len = frame->len;

            receive(&m_rx_buffer[m_rx_buffer_pos + 4], frame_len);

            m_rx_buffer_pos = (m_rx_buffer_pos + (frame_len + 4 + 3)) & BUFFER_MASK;

            CPU::outw(m_io_base + CAPR, m_rx_buffer_pos - 0x10);
        }
    }
};

class RTL8139Driver : public Tree::Driver {
    public:
    RTL8139Driver() : Driver("rtl8139") {}
    ~RTL8139Driver() {}

    bool check(Tree::Device* device) {
        auto pci_device = (PCIDevice*) device;

        if (pci_device->p_class != 0x02 || pci_device->subclass != 0x00 ||
            pci_device->vendor_id != 0x10EC || pci_device->device_id != 0x8139)
            return false;

        return true;
    }

    void bind(Tree::Device* device) {
        auto pci_device = (PCIDevice*) device;

        set_busmaster(pci_device, true);

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