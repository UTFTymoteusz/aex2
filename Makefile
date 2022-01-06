ARCH ?= x86_64

ISO := iso/

ROOT_DIR      := $(shell pwd)/$(ISO)
KERNELMOD_DIR := $(ROOT_DIR)sys/mod/core/
INITMOD_DIR   := $(ROOT_DIR)sys/mod/init/

CROSSGCCPATH := $(shell which x86_64-pc-aex2-gcc)
KERNEL_SRC   := $(shell pwd)/kernel/

CORES := 2
RAM   := 64M

DEBUG := 1

.PHONY: iso

all:
	@mkdir -p "$(shell pwd)/$(ISO)"
	@mkdir -p "$(shell pwd)/$(ISO)bin/"
	@mkdir -p "$(shell pwd)/$(ISO)boot/"
	@mkdir -p "$(shell pwd)/$(ISO)lib/"
	@mkdir -p "$(shell pwd)/$(ISO)proc/"
	@mkdir -p "$(shell pwd)/$(ISO)sys/"
	@mkdir -p "$(shell pwd)/$(ISO)sys/mod/"
	@mkdir -p "$(shell pwd)/$(ISO)sys/mod/core/"
	@mkdir -p "$(shell pwd)/$(ISO)sys/mod/init/"
	@mkdir -p "$(shell pwd)/$(ISO)sys/sym/"
	
	@cd mod     && $(MAKE) -s all KERNELMOD_DIR="$(KERNELMOD_DIR)" KERNEL_SRC="$(KERNEL_SRC)" ARCH="$(ARCH)" DEBUG="$(DEBUG)"
	@cd kernel  && $(MAKE) -s all -j 8 ROOT_DIR="$(ROOT_DIR)"      ARCH="$(ARCH)" DEBUG="$(DEBUG)"
	
	@cd kernel  && $(MAKE) -s copy ROOT_DIR="$(ROOT_DIR)"

ifneq (,$(wildcard $(CROSSGCCPATH)))
	@cd libc    && $(MAKE) -s all copy install COPY_DIR="$(ROOT_DIR)lib/" ARCH="$(ARCH)"

	@cd bin     && $(MAKE) -s all copy COPY_DIR="$(ROOT_DIR)bin/" CC="x86_64-pc-aex2-gcc"
	@cd init    && $(MAKE) -s all copy COPY_DIR="$(ROOT_DIR)sys/" CC="x86_64-pc-aex2-gcc"
	@cd utest   && $(MAKE) -s all copy COPY_DIR="$(ROOT_DIR)sys/" CC="x86_64-pc-aex2-gcc"
	@cd manbong && $(MAKE) -s all copy COPY_DIR="$(ROOT_DIR)bin/" CC="x86_64-pc-aex2-gcc"
else
	@echo x86_64-pc-aex2-gcc not found, skipping building any userspace binaries
endif

iso:
	grub-mkrescue -o /tmp/aex2/aex.iso $(ISO)

fat32:
	./mkfat32.sh

atftp:
	cp -ru $(ISO)boot /srv/atftp/
	cp -ru $(ISO)sys  /srv/atftp/

runnet:
	qemu-system-x86_64 -monitor stdio -debugcon /dev/stderr -machine type=q35 -smp $(CORES) -m $(RAM) \
	-cdrom /tmp/aex2/aex.iso --enable-kvm \
	-netdev tap,id=net0,ifname=tap0,script=no,downscript=no \
	-device rtl8139,netdev=net0,mac=00:01:e3:00:02:00
	
runfat:
	qemu-system-x86_64 -monitor stdio -debugcon /dev/stderr -machine type=q35 -smp $(CORES) -m $(RAM) \
	-drive file=/tmp/aex2/aex.fat32.img,format=raw --enable-kvm

run:
	qemu-system-x86_64 -monitor stdio -debugcon /dev/stderr -machine type=q35 -smp $(CORES) -m $(RAM) \
	-cdrom /tmp/aex2/aex.iso --enable-kvm $(EXTRA_FLAGS)

rungdb:
	qemu-system-x86_64 -monitor stdio -debugcon /dev/stderr -machine type=q35 -smp 1 -m $(RAM) \
	-cdrom /tmp/aex2/aex.iso --enable-kvm $(EXTRA_FLAGS) -s -S

clean:
	@cd mod     && $(MAKE) -s clean
	@cd kernel  && $(MAKE) -s clean
	@cd libc    && $(MAKE) -s clean
	@cd init    && $(MAKE) -s clean
	@cd bin     && $(MAKE) -s clean
	@cd utest   && $(MAKE) -s clean
	@cd manbong && $(MAKE) -s clean

	rm -rf $(ISO)bin/
	rm -rf $(ISO)sys/mod/core/
	rm -rf $(ISO)sys/mod/init/

cleanusr:
	cd mod    && $(MAKE) -s clean
	cd libc   && $(MAKE) -s clean
	cd init   && $(MAKE) -s clean
	cd bin    && $(MAKE) -s clean

	rm -rf $(ISO)bin/

git:
	git submodule init
	git submodule update

format:
	cd mod    && $(MAKE) -s format
	cd kernel && $(MAKE) -s format
	cd libc   && $(MAKE) -s format
	cd init   && $(MAKE) -s format
	cd utest  && $(MAKE) -s format
	cd bin    && $(MAKE) -s format