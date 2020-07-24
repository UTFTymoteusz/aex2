#pragma once

#include "aex/fs/controlblock.hpp"
#include "aex/fs/filesystem.hpp"
#include "aex/optional.hpp"

namespace AEX::FS {
    class ISO9660 : public Filesystem {
        public:
        static void init();

        ISO9660();
        ~ISO9660();

        optional<ControlBlock*> mount(const char* source);

        private:
    };
}