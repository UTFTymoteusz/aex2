ISO := iso/

SYSTEM_DIR    := $(shell pwd)/$(ISO)sys/
KERNELMOD_DIR := $(SYSTEM_DIR)core/

.PHONY: iso

format:
	cd coremod && $(MAKE) format
	cd archmod && $(MAKE) format
	cd kernel  && $(MAKE) format

all:
	mkdir -p "$(shell pwd)/$(ISO)"
	mkdir -p "$(shell pwd)/$(ISO)sys/"
	mkdir -p "$(shell pwd)/$(ISO)sys/core/"
	mkdir -p "$(shell pwd)/$(ISO)boot/"
	
	cd coremod && $(MAKE) all KERNELMOD_DIR="$(KERNELMOD_DIR)"
	cd archmod && $(MAKE) all KERNELMOD_DIR="$(KERNELMOD_DIR)"
	cd kernel  && $(MAKE) all

	cd kernel && $(MAKE) copy SYSTEM_DIR="$(SYSTEM_DIR)"

iso:
	grub-mkrescue -o aex.iso $(ISO) 2> /dev/null

runnet:
	qemu-system-x86_64 -monitor stdio -machine type=q35 -smp 4 -m 32M -cdrom aex.iso \
	-netdev tap,id=net0,ifname=TAP -device rtl8139,netdev=net0,mac=00:01:e3:00:00:00 	   \
	--enable-kvm
	
run:
	qemu-system-x86_64 -monitor stdio --debugcon stdio -machine type=q35 -smp 4 -m 32M -cdrom aex.iso

clean:
	cd coremod && $(MAKE) clean
	cd archmod && $(MAKE) clean
	cd kernel  && $(MAKE) clean

	rm -rf $(ISO)sys/core/

git:
	git submodule init
	git submodule update