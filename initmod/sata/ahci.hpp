#pragma once

#include <stddef.h>
#include <stdint.h>

namespace AEX::Dev::SATA {
    class AHCI {
        public:
        typedef volatile uint32_t vuint32_t;
        typedef volatile uint64_t vuint64_t;

        static constexpr auto DEV_BUSY = 0x80;
        static constexpr auto DEV_DRQ  = 0x08;

        enum fis_type {
            REG_H2D   = 0x27,
            REG_D2H   = 0x34,
            DMA_ACT   = 0x39,
            DMA_SETUP = 0x41,
            DATA      = 0x46,
            BIST      = 0x58,
            PIO_SETUP = 0x5F,
            DEV_BITS  = 0xA1,
        };

        enum ata_command {
            READ_DMA_EXT           = 0x25,
            WRITE_DMA_EXT          = 0x35,
            PACKET                 = 0xA0,
            IDENTIFY_PACKET_DEVICE = 0xA1,
            IDENTIFY_DEVICE        = 0xEC,
        };

        enum scsi_command {
            READ_CAPACITY_10 = 0x25,
            READ_12          = 0xA8,
            WRITE_12         = 0xAA,
        };

        struct hba_port_t {
            vuint64_t command_list_address; // 0x00
            vuint64_t fis_address;          // 0x08

            vuint32_t interrupt_status; // 0x10
            vuint32_t interrupt_enable; // 0x14

            vuint32_t command_and_status; // 0x18
            vuint32_t reserved0;          // 0x1C
            vuint32_t task_file_data;     // 0x20
            vuint32_t signature;          // 0x24

            vuint32_t sata_status;       // 0x28
            vuint32_t sata_control;      // 0x2C
            vuint32_t sata_error;        // 0x30
            vuint32_t sata_active;       // 0x34
            vuint32_t command_issue;     // 0x38
            vuint32_t sata_notification; // 0x3C

            vuint32_t fis_control;
            vuint32_t reserved1[11];
            vuint32_t vendor_specific[4];
        } __attribute__((packed));

        struct hba_t {
            vuint32_t host_capability;
            vuint32_t global_host_control;
            vuint32_t interrupt_status;
            vuint32_t port_implemented;
            vuint32_t version;

            vuint32_t ccc_ctl;
            vuint32_t ccc_pts;
            vuint32_t em_loc;
            vuint32_t em_ctl;

            vuint32_t host_capability_ext;
            vuint32_t handoff_control;

            uint8_t reserved[0xA0 - 0x2C];
            uint8_t vendor_specific[0x100 - 0xA0];

            hba_port_t ports[];
        } __attribute__((packed));

        struct hba_command_header {
            uint8_t fis_length : 5;
            bool    atapi : 1;
            bool    write : 1;
            bool    prefetchable : 1;

            bool reset : 1;
            bool BIST : 1;
            bool clear_busy : 1;

            uint8_t reserved0 : 1;
            uint8_t port_multiplier_port : 4;

            uint16_t phys_region_table_len;

            volatile uint32_t phys_region_table_transferred;

            uint64_t command_table_addr;

            uint32_t reserved1[4];
        } __attribute__((packed));

        struct phys_region_table_entry {
            uint64_t data_address;
            uint32_t reserved0;

            uint32_t bytes : 22;
            uint32_t reserved1 : 9;
            uint32_t interrupt_on_completion : 1;
        } __attribute__((packed));

        struct fis_reg_h2d {
            uint8_t fis_type;

            uint8_t pm_port : 4;
            uint8_t reserved0 : 3;
            bool    command_control : 1;

            uint8_t command;
            uint8_t feature_low;

            uint8_t lba0;
            uint8_t lba1;
            uint8_t lba2;
            uint8_t device;

            uint8_t lba3;
            uint8_t lba4;
            uint8_t lba5;
            uint8_t feature_high;

            uint16_t count;
            uint8_t  isochronous;
            uint8_t  control;

            uint8_t reserved1[4];
        } __attribute__((packed));

        struct fis_reg_d2h {
            uint8_t fis_type;

            uint8_t pm_port : 4;
            uint8_t reserved0 : 2;
            bool    interrupt : 1;
            bool    reserved1 : 1;

            uint8_t status;
            uint8_t error;

            uint8_t lba0;
            uint8_t lba1;
            uint8_t lba2;
            uint8_t device;

            uint8_t lba3;
            uint8_t lba4;
            uint8_t lba5;
            uint8_t reserved2;

            uint16_t count;
            uint8_t  reserved3[2];

            uint8_t reserved4[4];
        } __attribute__((packed));

        struct fis_data {
            uint8_t fis_type;

            uint8_t pm_port : 4;
            uint8_t reserved0 : 4;

            uint8_t reserved1[2];

            uint8_t data[];
        } __attribute__((packed));

        struct fis_pio_setup {
            uint8_t fis_type;

            uint8_t pm_port : 4;
            uint8_t reserved0 : 1;
            bool    direction : 1;
            bool    interrupt : 1;
            bool    reserved1 : 1;

            uint8_t status;
            uint8_t error;

            uint8_t lba0;
            uint8_t lba1;
            uint8_t lba2;
            uint8_t device;

            uint8_t lba3;
            uint8_t lba4;
            uint8_t lba5;
            uint8_t reserved2;

            uint16_t count;
            uint8_t  reserved;
            uint8_t  new_status;

            uint16_t transfer_count;
            uint8_t  reserved4[2];
        } __attribute__((packed));

        struct fis_dma_setup {
            uint8_t fis_type;

            uint8_t pm_port : 4;
            uint8_t reserved0 : 1;
            bool    direction : 1;
            bool    interrupt : 1;
            bool    auto_activate : 1;

            uint8_t reserved1[2];

            uint64_t dma_buffer_id;

            uint32_t reserved2;

            uint32_t dma_buffer_offset;
            uint32_t transfer_count;
            uint32_t reserved3;
        } __attribute__((packed));

        struct hba_command_table {
            union {
                uint8_t     command_fis[64];
                fis_reg_h2d fis_reg_h2d_data;
            };

            uint8_t atapi_command[16];
            uint8_t reserved[48];

            phys_region_table_entry entries[];
        } __attribute__((packed));

        struct hba_fis {
            fis_dma_setup dma_setup;
            uint8_t       pad0[4];

            fis_pio_setup pio_setup;
            uint8_t       pad1[12];

            fis_reg_d2h reg_d2h;
            uint8_t     pad2[4];

            uint8_t dev_bits[8];

            uint8_t idk[64];

            uint8_t reserved[0x100 - 0xA0];
        } __attribute__((packed));

        AHCI(void* addr, int index);

        private:
        static constexpr auto POWER_MANAGEMENT_ACTIVE = 0x01;
        static constexpr auto DETECTION_PRESENT       = 0x03;

        static constexpr auto SATA_SIG_ATA   = 0x00000101;
        static constexpr auto SATA_SIG_ATAPI = 0xEB140101;
        static constexpr auto SATA_SIG_SEMB  = 0xC33C0101;
        static constexpr auto SATA_SIG_PM    = 0x96690101;

        hba_t* hba;
        int    index;

        int command_slots = 0;

        void scan_ports();

        void scan_port(hba_port_t* port, int port_index);
    };
}