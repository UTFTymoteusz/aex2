#include "aex/byte.hpp"
#include "aex/dev.hpp"
#include "aex/dev/tree.hpp"
#include "aex/printk.hpp"

#include "satadevice.hpp"

using namespace AEX::Dev;

namespace AEX::Sys::SATA {
    class SRBlock : public BlockDevice {
        public:
        SRBlock(SATADevice* device)
            : BlockDevice(device->name, SECTOR_SIZE, device->sector_count, device->max_page_burst) {
            m_device = device;
        }

        private:
        static constexpr auto SECTOR_SIZE = 2048;

        SATADevice* m_device;

        int64_t readBlock(void* buffer, uint64_t sector, uint32_t sector_count) {
            readWrite(buffer, sector, sector_count, false);
            return sector_count * SECTOR_SIZE;
        }

        int64_t writeBlock(const void* buffer, uint64_t sector, uint32_t sector_count) {
            readWrite((void*) buffer, sector, sector_count, true);
            return sector_count * SECTOR_SIZE;
        }

        void readWrite(void* buffer, uint32_t sector, uint32_t sector_count, bool write) {
            uint8_t packet[12] = {write ? AHCI::scsi_command::WRITE_12
                                        : AHCI::scsi_command::READ_12};

            *((uint32_t*) &(packet[2])) = bswap(sector);
            *((uint32_t*) &(packet[6])) = bswap(sector_count);

            m_device->scsiPacket(packet, buffer, sector_count * SECTOR_SIZE);
        }
    };

    class SRDriver : public Tree::Driver {
        public:
        SRDriver() : Driver("sr") {}

        bool check(Tree::Device* m_device) {
            auto device = (SATADevice*) m_device;
            return device->type == SATA_ATAPI;
        }

        void bind(Tree::Device* m_device) {
            auto device = (SATADevice*) m_device;

            auto block = new SRBlock(device);
            if (!block->registerDevice()) {
                printk("sr: %s: Failed to register device\n", device->name);
                delete block;
            }
        }

        private:
        static constexpr auto SECTOR_SIZE = 2048;
    };

    void sr_init() {
        register_driver("sata", new SRDriver());
    }
}