ISO := iso/

SYSTEM_DIR    := $(shell pwd)/$(ISO)sys/
KERNELMOD_DIR := $(SYSTEM_DIR)core/
INITMOD_DIR   := $(SYSTEM_DIR)init/

CROSSGCCPATH := $(shell which x86_64-aex2-elf-gcc)

.PHONY: iso

format:
	cd coremod && $(MAKE) format
	cd archmod && $(MAKE) format
	cd initmod && $(MAKE) format
	cd kernel  && $(MAKE) format
	cd libc    && $(MAKE) format
	cd init    && $(MAKE) format

all:
	mkdir -p "$(shell pwd)/$(ISO)"
	mkdir -p "$(shell pwd)/$(ISO)sys/"
	mkdir -p "$(shell pwd)/$(ISO)sys/core/"
	mkdir -p "$(shell pwd)/$(ISO)sys/init/"
	mkdir -p "$(shell pwd)/$(ISO)boot/"
	
	cd coremod && $(MAKE) all KERNELMOD_DIR="$(KERNELMOD_DIR)"
	cd archmod && $(MAKE) all KERNELMOD_DIR="$(KERNELMOD_DIR)"
	cd initmod && $(MAKE) all INITMOD_DIR="$(INITMOD_DIR)"
	cd kernel  && $(MAKE) all -j 8
	cd libc    && $(MAKE) all install
	
	cd kernel && $(MAKE) copy SYSTEM_DIR="$(SYSTEM_DIR)"

ifndef CROSSGCCPATH
	@echo x86_64-aex2-elf-gcc not found, skipping building any userspace binaries
	@exit 0
endif

	cd init && $(MAKE) all copy COPY_DIR="$(SYSTEM_DIR)"

iso:
	grub-mkrescue -o aex.iso $(ISO) 2> /dev/null

runnet:
	qemu-system-x86_64 -monitor stdio -machine type=q35 -smp 4 -m 32M -cdrom aex.iso \
	-netdev tap,id=net0,ifname=TAP -device rtl8139,netdev=net0,mac=00:01:e3:00:00:00 	   \
	--enable-kvm
	
run:
	qemu-system-x86_64 -monitor stdio -debugcon /dev/stderr -machine type=q35 -smp 4 -m 32M -cdrom aex.iso --enable-kvm

clean:
	cd coremod && $(MAKE) clean
	cd archmod && $(MAKE) clean
	cd initmod && $(MAKE) clean
	cd kernel  && $(MAKE) clean
	cd libc    && $(MAKE) clean
	cd init    && $(MAKE) clean

	rm -rf $(ISO)sys/core/
	rm -rf $(ISO)sys/init/

git:
	git submodule init
	git submodule update