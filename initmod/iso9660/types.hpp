#pragma once

#include <stddef.h>
#include <stdint.h>

namespace AEX::FS {
    enum ios9960_vd_type : uint8_t {
        BOOT_RECORD                     = 0,
        PRIMARY_VOLUME_DESCRIPTOR       = 1,
        SUPPLEMENTARY_VOLUME_DESCRIPTOR = 2,
        VOLUME_PARTITON_DESCRIPTOR      = 3,
        TERMINATOR                      = 255,
    };

    struct iso9660_int16lebe {
        int16_t le;
        int16_t be;
    } __attribute__((packed));

    struct iso9660_uint16lebe {
        uint16_t le;
        uint16_t be;
    } __attribute__((packed));

    struct iso9660_int32lebe {
        int32_t le;
        int32_t be;
    } __attribute__((packed));

    struct iso9660_uint32lebe {
        uint32_t le;
        uint32_t be;
    } __attribute__((packed));

    struct iso9660_int64lebe {
        int64_t le;
        int64_t be;
    } __attribute__((packed));

    struct iso9660_uint64lebe {
        uint64_t le;
        uint64_t be;
    } __attribute__((packed));

    struct iso9660_dentry_date_time {
        uint8_t years;
        uint8_t month;
        uint8_t day;
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
        int8_t  timezone;
    } __attribute__((packed));

    struct iso9660_dentry {
        uint8_t len;
        uint8_t ext_len;

        iso9660_uint32lebe data_lba;
        iso9660_uint32lebe data_len;

        iso9660_dentry_date_time date_time;

        uint8_t flags;
        uint8_t unit_size;
        uint8_t interleave_size;

        iso9660_uint16lebe seq_number;

        uint8_t name_len;
        char    name[];

        bool isDirectory() {
            return (flags & (1 << 1)) > 0;
        }
    } __attribute__((packed));

    struct iso9660_vd_header {
        ios9960_vd_type type;
        char            identifier[5];
        uint8_t         version;
    } __attribute__((packed));

    struct iso9660_boot_record {
        iso9660_vd_header header;
        char              boot_system_identifier[32];
        char              boot_identifier[32];
        uint8_t           custom[1977];
    } __attribute__((packed));

    struct iso9660_primary_volume_descriptor {
        iso9660_vd_header header;
        uint8_t           unused0;

        char system_identifier[32];
        char volume_identifier[32];

        uint8_t unused1[8];

        iso9660_uint32lebe volume_blocks;

        uint8_t unused2[32];

        iso9660_uint16lebe volume_set_size;
        iso9660_uint16lebe volume_sequence_size;
        iso9660_uint16lebe logical_block_size;
        iso9660_uint32lebe path_table_size;

        uint32_t lpath_table_lba;
        uint32_t optional_lpath_table_lba;

        uint32_t mpath_table_lba;
        uint32_t optional_mpath_table_lba;

        uint8_t root_dentry_bytes[34];

        char volume_set_identifier[128];
        char publisher_identifier[128];
        char data_preparer_identifier[128];
        char application_identifier[128];
        char copyright_file_identifier[38];
        char abstract_identifier[36];
        char bibliographic_identifier[37];

    } __attribute__((packed));

    struct iso9660_terminator {
        iso9660_vd_header header;
    } __attribute__((packed));
}