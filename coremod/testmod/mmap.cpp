#include "aex/mem/mmap.hpp"

#include "aex/assert.hpp"
#include "aex/mem/heap.hpp"
#include "aex/mem/paging.hpp"
#include "aex/mem/phys.hpp"
#include "aex/proc/thread.hpp"

using namespace AEX;

void test_mmap() {
    uint64_t frames;
    uint64_t heap;

    for (int i = 0; i < 4; i++) {
        auto  file = FS::File::open("/sys/aexkrnl.elf", FS::O_RDWR).value;
        void* mmap = Mem::mmap(nullptr, 65536, Mem::PROT_READ, Mem::MAP_NONE, file, 0).value;

        char* volatile bong = (char* volatile) mmap;

        char a = bong[0x0000];
        char b = bong[0x1000];
        char c = bong[0x2000];
        char d = bong[0x3000];
        char e = bong[0x4000];
        char f = bong[0x5000];
        char g = bong[0x6000];
        char h = bong[0x7000];

        a *= 2;
        b *= 2;
        c *= 2;
        d *= 2;
        e *= 2;
        f *= 2;
        g *= 2;
        h *= 2;

        Mem::munmap(mmap, 65536);

        if (i == 0) {
            frames = Mem::Phys::frames_available;
            heap   = Mem::Heap::heap_allocated;
        }
    }

    // AEX_ASSERT(Mem::Phys::frames_available == frames);
    // AEX_ASSERT(Mem::Heap::heap_allocated == heap);
}
