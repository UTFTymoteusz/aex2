#include "satadevice.hpp"

#include "aex/arch/sys/cpu.hpp"
#include "aex/byte.hpp"
#include "aex/math.hpp"
#include "aex/mem.hpp"
#include "aex/printk.hpp"

using namespace AEX::Dev;

namespace AEX::Sys::SATA {
    bool SATADevice::init() {
        int slot = findSlot();

        auto header = getHeader(slot);
        auto table  = getTable(slot);

        header->fis_length = sizeof(AHCI::fis_reg_h2d) / sizeof(uint32_t);
        header->write      = false;
        header->atapi      = false;

        header->phys_region_table_transferred = 0;
        header->phys_region_table_len         = max_prts;

        memset((void*) &(table->fis_reg_h2d_data), '\0', sizeof(AHCI::fis_reg_h2d));

        table->fis_reg_h2d_data.command =
            atapi ? AHCI::ata_command::IDENTIFY_PACKET_DEVICE : AHCI::ata_command::IDENTIFY_DEVICE;
        table->fis_reg_h2d_data.command_control = true;
        table->fis_reg_h2d_data.fis_type        = AHCI::fis_type::REG_H2D;

        uint8_t buffer[512];
        memset(buffer, '\0', sizeof(buffer));

        fillPRDTs(table, buffer, 512);

        while (hba_port->task_file_data & (AHCI::DEV_BUSY | AHCI::DEV_DRQ))
            ;

        startCMD();
        issueCMD(slot);

        releaseSlot(slot);

        uint16_t* identify = (uint16_t*) buffer;
        char      flipped_buffer[42];

        memset(flipped_buffer, '\0', sizeof(flipped_buffer));
        memcpy(flipped_buffer, &identify[27], 40);

        for (int i = 0; i < 40; i += 2) {
            char temp = flipped_buffer[i];

            flipped_buffer[i]     = flipped_buffer[i + 1];
            flipped_buffer[i + 1] = temp;
        }

        if (!atapi)
            sector_count = *((uint64_t*) (&identify[100]));
        else {
            uint8_t              packet[12] = {AHCI::scsi_command::READ_CAPACITY_10};
            big_endian<uint32_t> buffer[2]  = {0};

            scsiPacket(packet, buffer, sizeof(buffer));

            buffer[0] = buffer[0];
            buffer[1] = buffer[1];

            sector_count = buffer[0];
        }

        printk("sata: %s: Model %s\n", this->name, flipped_buffer);
        printk("sata: %s: %li sectors\n", this->name, this->sector_count);

        return true;
    }

    void SATADevice::startCMD() {
        hba_port->command_and_status &= ~PxCMD_ST;

        while (hba_port->command_and_status & PxCMD_CR)
            ;

        hba_port->command_and_status |= PxCMD_FRE;
        hba_port->command_and_status |= PxCMD_ST;
    }

    void SATADevice::stopCMD() {
        hba_port->command_and_status &= ~PxCMD_ST;

        while (hba_port->command_and_status & PxCMD_CR)
            ;

        hba_port->command_and_status &= ~PxCMD_FRE;
    }

    bool SATADevice::issueCMD(int slot) {
        while (hba_port->task_file_data & (AHCI::DEV_BUSY | AHCI::DEV_DRQ))
            ;

        size_t counter = 0;

        while (true) {
            hba_port->command_issue |= (1 << slot);

            if (counter++ > 100000000)
                kpanic("sata: issueCMD() stuck");

            if (hba_port->interrupt_status & (1 << 30))
                return false;

            if (!(hba_port->command_issue & (1 << slot)))
                break;
        }

        return true;
    }

    int SATADevice::findSlot() {
        while (true) {
            ScopeSpinlock scopeLock(m_lock);

            for (int i = 0; i < max_commands; i++)
                if (!(m_command_slots & (1 << i))) {
                    m_command_slots |= (1 << i);
                    return i;
                }
        }
    }

    void SATADevice::releaseSlot(int slot) {
        ScopeSpinlock scopeLock(m_lock);

        m_command_slots &= ~(1 << slot);
    }

    void SATADevice::fillPRDTs(volatile AHCI::hba_command_table* table, void* dst, size_t len) {
        size_t dsti      = (size_t) dst;
        size_t page_size = Sys::CPU::PAGE_SIZE;

        int index = 0;

        while (len > 0) {
            size_t aligned_next = int_floor<size_t>(dsti + page_size, page_size);
            size_t llen         = min(aligned_next - dsti, len);

            table->entries[index].bytes        = llen - 1;
            table->entries[index].data_address = Mem::kernel_pagemap->paddrof((void*) dsti);
            index++;

            dsti += llen;
            len -= llen;
        }
    }

    void SATADevice::scsiPacket(uint8_t* packet, void* buffer, int len) {
        int slot = findSlot();

        auto header = getHeader(slot);
        auto table  = getTable(slot);

        header->fis_length = sizeof(AHCI::fis_reg_h2d) / sizeof(uint32_t);
        header->write      = false;
        header->atapi      = true;

        header->phys_region_table_transferred = 0;

        fillPRDTs(table, buffer, len);

        memcpy(table->atapi_command, packet, 16);

        auto fis = &table->fis_reg_h2d_data;

        fis->command         = AHCI::ata_command::PACKET;
        fis->command_control = true;
        fis->fis_type        = AHCI::fis_type::REG_H2D;

        // idk this makes it magically work out of a sudden
        fis->lba1 = 0x0008;
        fis->lba2 = 0x0008;

        issueCMD(slot);
        releaseSlot(slot);
    }

    AHCI::hba_command_header* SATADevice::getHeader(int slot) {
        return &command_headers[slot];
    }

    AHCI::hba_command_table* SATADevice::getTable(int slot) {
        return (AHCI::hba_command_table*) ((size_t) command_tables + 8192 * slot);
    }
}