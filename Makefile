ARCH ?= x64

ISO := iso/

ROOT_DIR      := $(shell pwd)/$(ISO)
KERNELMOD_DIR := $(ROOT_DIR)sys/core/
INITMOD_DIR   := $(ROOT_DIR)sys/init/

CROSSGCCPATH := $(shell which x86_64-aex2-elf-gcc)
KERNEL_SRC   := $(shell pwd)/kernel/

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
	
	cd mod    && $(MAKE) all KERNELMOD_DIR="$(KERNELMOD_DIR)" KERNEL_SRC="$(KERNEL_SRC)" ARCH="$(ARCH)"
	cd kernel && $(MAKE) all -j 8 ROOT_DIR="$(ROOT_DIR)"      ARCH="$(ARCH)"
	cd libc   && $(MAKE) all install 						  ARCH="$(ARCH)"
	
	cd kernel && $(MAKE) copy ROOT_DIR="$(ROOT_DIR)"

ifndef CROSSGCCPATH
	@echo x86_64-aex2-elf-gcc not found, skipping building any userspace binaries
	@exit 0
endif

	cd init && $(MAKE) all copy COPY_DIR="$(ROOT_DIR)sys/"

iso:
	grub-mkrescue -o aex.iso $(ISO) 2> /dev/null

runnet:
	qemu-system-x86_64 -monitor stdio -machine type=q35 -smp 1 -m 32M -cdrom aex.iso \
	-netdev tap,id=net0,ifname=TAP -device rtl8139,netdev=net0,mac=00:01:e3:00:00:00 \
	--enable-kvm
	
run:
	qemu-system-x86_64 -monitor stdio -debugcon /dev/stderr -machine type=q35 -smp 1 -m 32M -cdrom aex.iso --enable-kvm

clean:
	cd mod    && $(MAKE) clean
	cd kernel && $(MAKE) clean
	cd libc   && $(MAKE) clean
	cd init   && $(MAKE) clean

	rm -rf $(ISO)sys/core/
	rm -rf $(ISO)sys/init/

git:
	git submodule init
	git submodule update