#include "aex/arch/sys/cpu.hpp"
#include "aex/debug.hpp"
#include "aex/dev/tree.hpp"
#include "aex/printk.hpp"

#include "common.hpp"

using namespace AEX;
using namespace AEX::Dev;
using CPU = AEX::Sys::CPU;

const char* MODULE_NAME = "ps2";

void kb_init();
void mouse_init();

void kb_irq();
void mouse_irq();

class PS2 : public Tree::Driver {
    public:
    PS2() : Driver("ps2") {}
    ~PS2() {}

    bool check(Tree::Device* device) {
        if (strcmp(device->name, "ps2") != 0)
            return false;

        return true;
    }

    void bind(Tree::Device*) {
        printk(PRINTK_INIT "ps2: Initializing\n");

        sendCommand(PS2_CTRL_CMD_DISABLE_PORT0);
        sendCommand(PS2_CTRL_CMD_DISABLE_PORT1);

        // Flushing buffers
        while (CPU::inb(PS2_IO_STATUS) & 0x01)
            CPU::inb(PS2_IO_DATA);

        uint8_t byte = sendCommandGetResponse(PS2_CTRL_CMD_READ_CFG_BYTE);

        byte &= ~(1 << 6);
        byte &= ~(1 << 3);
        byte &= ~(0x03);

        sendCommand(PS2_CTRL_CMD_WRITE_CFG_BYTE, byte);

        sendCommand(PS2_CTRL_CMD_ENABLE_PORT0);
        kb_init();

        byte = sendCommandGetResponse(PS2_CTRL_CMD_READ_CFG_BYTE);
        byte |= 0x3;

        sendCommand(PS2_CTRL_CMD_WRITE_CFG_BYTE, byte);

        printk(PRINTK_OK "ps2: Initialized\n");
    }

    private:
    void sendCommand(uint8_t cmd) {
        while (CPU::inb(PS2_IO_STATUS) & 0x02)
            ;

        CPU::outb(PS2_IO_COMMAND, cmd);
    }

    void sendCommand(uint8_t cmd, uint8_t byte) {
        while (CPU::inb(PS2_IO_STATUS) & 0x02)
            ;

        CPU::outb(PS2_IO_COMMAND, cmd);

        while (CPU::inb(PS2_IO_STATUS) & 0x02)
            ;

        CPU::outb(PS2_IO_DATA, byte);
    }

    uint8_t sendCommandGetResponse(uint8_t cmd) {
        sendCommand(cmd);
        while (!(CPU::inb(PS2_IO_STATUS) & 0x01))
            ;

        return CPU::inb(PS2_IO_DATA);
    }
};

PS2* driver;

void module_enter() {
    driver = new PS2();

    if (!Tree::register_driver("main", driver)) {
        printk(PRINTK_WARN "ps2: Failed to register the driver\n");

        delete driver;
        return;
    }
}

void module_exit() {}