#pragma once

#include "aex/dev.hpp"
#include "aex/errno.hpp"
#include "aex/fs/controlblock.hpp"
#include "aex/fs/file.hpp"
#include "aex/fs/inode.hpp"
#include "aex/optional.hpp"

#include "types.hpp"

constexpr auto BLOCK_SIZE = 2048;

namespace AEX::FS {
    class ISO9660ControlBlock : public ControlBlock {
        public:
        ISO9660ControlBlock(Dev::BlockDevice_SP block, const iso9660_dentry& root_dentry) {
            _root_dentry = root_dentry;

            block_dev     = block;
            block_size    = BLOCK_SIZE;
            root_inode_id = root_dentry.data_lba.le;
        }

        optional<INode_SP> getINode(INode_SP dir, dir_entry dentry, int id);

        private:
        iso9660_dentry _root_dentry;
    };
}