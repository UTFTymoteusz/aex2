format:
	cd coremod && $(MAKE) format
	cd kernel && $(MAKE) format

all:
	mkdir -p "$(shell pwd)/kernel/bin/grubiso/sys/core/"
	
	cd coremod && $(MAKE) all COREMOD_DIR="$(shell pwd)/kernel/bin/grubiso/sys/core/"
	cd kernel && $(MAKE) all

iso:
	cd kernel && $(MAKE) iso

run:
	cd kernel && $(MAKE) qemu

runnet:
	cd kernel && $(MAKE) qemunet

clean:
	cd coremod && $(MAKE) clean
	cd kernel && $(MAKE) clean

git:
	git submodule init
	git submodule update