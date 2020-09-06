#include "aex/dev/tree.hpp"
#include "aex/printk.hpp"

#include "satadevice.hpp"

using namespace AEX::Dev;

namespace AEX::Sys::SATA {
    class SDDriver : public Tree::Driver {
        public:
        SDDriver() : Driver("sd") {}

        bool check(Tree::Device* m_device) {
            auto device = (SATADevice*) m_device;
            return device->type == SATA_ATA;
        }

        void bind(Tree::Device*) {
            // auto device = (SATADevice*) m_device;
        }
    };

    void sd_init() {
        register_driver("sata", new SDDriver());
    }
}