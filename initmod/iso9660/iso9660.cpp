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
    ISO9660* iso9660_fs;

    void ISO9660::init() {
        iso9660_fs = new ISO9660();
    }

    ISO9660::ISO9660() : Filesystem("iso9660") {}
    ISO9660::~ISO9660() {}

    optional<ControlBlock*> ISO9660::mount(const char* source) {
        printk("iso9660: Mount attempt from %s\n", source);

        if (!source) {
            printk(PRINTK_WARN "iso9660: mount: Source not set\n");
            return EINVAL;
        }

        auto info = File::info(source);
        if (!info) {
            printk(PRINTK_WARN "iso9660: mount: Failed to get file info\n");
            return info.error_code;
        }

        if (!info.value.is_block()) {
            printk(PRINTK_WARN "iso9660: mount: Source is not a block device\n");
            return ENOTBLK;
        }

        auto handle_try = Dev::open_block_handle(info.value.dev_id);
        if (!handle_try) {
            printk(PRINTK_WARN "iso9660: mount: Failed to open the block device\n");
            return ENOENT;
        }

        auto handle = handle_try.value;

        iso9660_dentry root_dentry;
        uint8_t        buffer[BLOCK_SIZE];

        for (int i = 0; i <= 64; i++) {
            if (i == 64) {
                printk(PRINTK_WARN "iso9660: Too many volume descriptors\n");
                return EINVAL;
            }

            handle.read(buffer, ISO_START + BLOCK_SIZE * i, BLOCK_SIZE);

            auto header = (iso9660_vd_header*) buffer;
            if (memcmp(header->identifier, IDENTIFIER, 5) != 0) {
                printk(PRINTK_WARN "iso9660: mount: Identifier mismatch\n");
                return EINVAL;
            }

            switch (header->type) {
            case TERMINATOR:
                i = 666; // a quick and dirty way to break out of this for loop in a switch
                break;
            case PRIMARY_VOLUME_DESCRIPTOR: {
                auto pvd = (iso9660_primary_volume_descriptor*) buffer;

                root_dentry = *((iso9660_dentry*) pvd->root_dentry_bytes);
            } break;
            default:
                break;
            }
        }

        return new ISO9660ControlBlock(handle, root_dentry);
    }
}