#include "iso9660.hpp"

#include "aex/dev.hpp"
#include "aex/fs/file.hpp"
#include "aex/printk.hpp"
#include "aex/string.hpp"

#include "controlblock.hpp"
#include "types.hpp"

#define IDENTIFIER "CD001"

constexpr auto ISO_START = BLOCK_SIZE * 0x10;

namespace AEX::FS {
    void ISO9660::init() {
        new ISO9660();
    }

    ISO9660::ISO9660() : Filesystem("iso9660") {}
    ISO9660::~ISO9660() {}

    optional<ControlBlock*> ISO9660::mount(const char* source) {
        printk("iso9660: Mount attempt from %s\n", source);

        if (!source)
            return EINVAL;

        auto info = File::info(source);
        if (!info.has_value)
            return info.error_code;

        if (!info.value.is_block())
            return ENOTBLK;

        auto block = (Dev::BlockDevice_SP) Dev::devices.get(info.value.dev_id);
        if (!block.isValid())
            return ENOENT;

        iso9660_dentry root_dentry;

        uint8_t buffer[BLOCK_SIZE];

        for (int i = 0; i <= 64; i++) {
            if (i == 64) {
                printk(PRINTK_WARN "iso9660: Too many volume descriptors\n");
                return EINVAL;
            }

            block->read(buffer, ISO_START + BLOCK_SIZE * i, BLOCK_SIZE);

            auto header = (iso9660_vd_header*) buffer;
            if (memcmp(header->identifier, IDENTIFIER, 5) != 0)
                return EINVAL;

            switch (header->type) {
            case ios9960_vd_type::TERMINATOR:
                i = 666; // a quick and dirty way to break out of this for loop in a switch
                break;
            case ios9960_vd_type::PRIMARY_VOLUME_DESCRIPTOR: {
                auto pvd = (iso9660_primary_volume_descriptor*) buffer;

                root_dentry = *((iso9660_dentry*) pvd->root_dentry_bytes);
            } break;
            default:
                break;
            }
        }

        return new ISO9660ControlBlock(block, root_dentry);
    }
}