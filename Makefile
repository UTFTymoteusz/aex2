ARCH ?= x86_64

ISO := iso/

ROOT_DIR      := $(shell pwd)/$(ISO)
KERNELMOD_DIR := $(ROOT_DIR)sys/mod/core/
INITMOD_DIR   := $(ROOT_DIR)sys/mod/init/

CROSSGCCPATH := $(shell which x86_64-aex2-elf-gcc)
KERNEL_SRC   := $(shell pwd)/kernel/

.PHONY: iso

format:
	cd mod    && $(MAKE) format
	cd kernel && $(MAKE) format
	cd libc   && $(MAKE) format
	cd init   && $(MAKE) format
	cd utest  && $(MAKE) format
	cd bin    && $(MAKE) format

all:
	mkdir -p "$(shell pwd)/$(ISO)"
	mkdir -p "$(shell pwd)/$(ISO)bin/"
	mkdir -p "$(shell pwd)/$(ISO)boot/"
	mkdir -p "$(shell pwd)/$(ISO)lib/"
	mkdir -p "$(shell pwd)/$(ISO)sys/"
	mkdir -p "$(shell pwd)/$(ISO)sys/mod/"
	mkdir -p "$(shell pwd)/$(ISO)sys/mod/core/"
	mkdir -p "$(shell pwd)/$(ISO)sys/mod/init/"
	
	cd mod    && $(MAKE) all KERNELMOD_DIR="$(KERNELMOD_DIR)" KERNEL_SRC="$(KERNEL_SRC)" ARCH="$(ARCH)"
	cd kernel && $(MAKE) all -j 8 ROOT_DIR="$(ROOT_DIR)"      ARCH="$(ARCH)"
	
	cd kernel && $(MAKE) copy ROOT_DIR="$(ROOT_DIR)"

ifneq (,$(wildcard $(CROSSGCCPATH)))
	cd libc  && $(MAKE) all copy install COPY_DIR="$(ROOT_DIR)lib/" ARCH="$(ARCH)"

	cd bin   && $(MAKE) all copy COPY_DIR="$(ROOT_DIR)bin/"
	cd init  && $(MAKE) all copy COPY_DIR="$(ROOT_DIR)sys/"
	cd utest && $(MAKE) all copy COPY_DIR="$(ROOT_DIR)sys/"
else
	@echo x86_64-aex2-elf-gcc not found, skipping building any userspace binaries
endif

iso:
	grub-mkrescue -o /tmp/aex.iso $(ISO)

runnet:
	qemu-system-x86_64 -monitor stdio -debugcon /dev/stderr -machine type=q35 -smp 1 -m 32M \
	-cdrom /tmp/aex.iso --enable-kvm \
	-netdev tap,id=net0,ifname=tap0 -device rtl8139,netdev=net0,mac=00:01:e3:00:00:00 \
	
run:
	qemu-system-x86_64 -monitor stdio -debugcon /dev/stderr -machine type=q35 -smp 2 -m 32M \
	-cdrom /tmp/aex.iso --enable-kvm

clean:
	cd mod    && $(MAKE) clean
	cd kernel && $(MAKE) clean
	cd libc   && $(MAKE) clean
	cd init   && $(MAKE) clean
	cd bin    && $(MAKE) clean

	rm -rf $(ISO)bin/
	rm -rf $(ISO)sys/mod/core/
	rm -rf $(ISO)sys/mod/init/

cleanusr:
	cd mod    && $(MAKE) clean
	cd libc   && $(MAKE) clean
	cd init   && $(MAKE) clean
	cd bin    && $(MAKE) clean

	rm -rf $(ISO)bin/

git:
	git submodule init
	git submodule update