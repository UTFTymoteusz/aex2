#include "controlblock.hpp"

#include "aex/fs.hpp"
#include "aex/math.hpp"
#include "aex/printk.hpp"
#include "aex/proc.hpp"
#include "aex/string.hpp"

#include "inode.hpp"
#include "types.hpp"

namespace AEX::FS {
    optional<INode_SP> ISO9660ControlBlock::getINode(INode_SP dir, dir_entry dentry, int id) {
        if (id == root_inode_id)
            return INode_SP(new ISO9600DirectoryINode(_root_dentry));

        auto _dir = (Mem::SmartPointer<ISO9600DirectoryINode>) dir;

        iso9660_dentry iso_dentry = {};

        // We can get away with doing it like this because we're read only
        dir->control_block->block_dev->read(
            &iso_dentry, _dir->_dentry.data_lba.le * BLOCK_SIZE + dentry.pos, sizeof(iso_dentry));

        if (dentry.is_directory())
            return INode_SP(new ISO9600DirectoryINode(iso_dentry));

        return INode_SP(new ISO9600FileINode(iso_dentry));
    }
}