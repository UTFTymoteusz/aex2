#include "aex/arch/sys/cpu.hpp"
#include "aex/dev/input.hpp"
#include "aex/dev/inputdevice.hpp"
#include "aex/printk.hpp"
#include "aex/proc.hpp"
#include "aex/sys/irq.hpp"

#include "common.hpp"
#include "translation.hpp"

#include <stddef.h>
#include <stdint.h>

using namespace AEX;
using namespace AEX::Dev;
using CPU = AEX::Sys::CPU;

class PS2Keyboard : public InputDevice {
    void updateLEDs(led_flag_t flags) {
        printk("ps2: update LEDs: 0x%02x\n", flags);
    }
};

PS2Keyboard* device = nullptr;

uint8_t sendCommand(uint8_t cmd);
void    sendCommand(uint8_t cmd, uint8_t sub);

void kb_irq(void*);

void kb_init() {
    device = new PS2Keyboard();
    if (!device->registerDevice()) {
        printk(PRINTK_WARN "ps2: Failed to register the keyboard device\n");

        delete device;
        return;
    }

    sendCommand(0xFF);
    sendCommand(0xF0, 2);
    sendCommand(0xF3, 0b00100000);
    sendCommand(0xF4);

    Sys::IRQ::register_handler(1, kb_irq);
}

void kb_irq(void*) {
    static bool released = false;
    static bool extra    = false;

    uint8_t byte = CPU::inb(PS2_IO_DATA);

    if (byte == 0xE0) {
        extra = true;
        return;
    }
    else if (byte == 0xF0) {
        released = true;
        return;
    }

    uint8_t translated = 0;

    translated = !extra ? translation_normal[byte] : translation_extra[byte];

    if (!translated) {
        released = false;
        extra    = false;

        return;
    }

    // printk("ps2: %s 0x%02x\n", released ? "released" : "pressed", translated);

    if (!released)
        device->keyPress((Input::hid_keycode_t) translated);
    else
        device->keyRelease((Input::hid_keycode_t) translated);

    released = false;
    extra    = false;
}

uint8_t sendCommand(uint8_t cmd) {
    uint8_t ret;

    for (int i = 0; i < 3; i++) {
        while (CPU::inb(PS2_IO_STATUS) & PS2_STATUS_INPUT_FULL)
            ;

        CPU::outb(PS2_IO_DATA, cmd);

        while (!(CPU::inb(PS2_IO_STATUS) & PS2_STATUS_OUTPUT_FULL))
            ;

        ret = CPU::inb(PS2_IO_DATA);
        if (ret == 0x00 || ret == 0xFE || ret == 0xFF)
            continue;

        break;
    }

    while (CPU::inb(PS2_IO_STATUS) & PS2_STATUS_OUTPUT_FULL)
        CPU::inb(PS2_IO_DATA);

    return ret;
}

void sendCommand(uint8_t cmd, uint8_t sub) {
    uint8_t ret;

    for (int i = 0; i < 3; i++) {
        while (CPU::inb(PS2_IO_STATUS) & PS2_STATUS_INPUT_FULL)
            ;

        CPU::outb(PS2_IO_DATA, cmd);

        while (CPU::inb(PS2_IO_STATUS) & PS2_STATUS_INPUT_FULL)
            ;

        CPU::outb(PS2_IO_DATA, sub);

        while (!(CPU::inb(PS2_IO_STATUS) & PS2_STATUS_OUTPUT_FULL))
            ;

        ret = CPU::inb(PS2_IO_DATA);
        if (ret == 0x00 || ret == 0xFE || ret == 0xFF)
            continue;

        break;
    }
}