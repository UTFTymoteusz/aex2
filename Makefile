format:
	cd coremod && $(MAKE) format
	cd kernel && $(MAKE) format

all:
	mkdir -p "$(shell pwd)/kernel/bin/grubiso/sys/core/"
	cd coremod && $(MAKE) all COPY_DIR="$(shell pwd)/kernel/bin/grubiso/sys/core/"
	cd kernel && $(MAKE) all

iso:
	cd kernel && $(MAKE) iso

run:
	cd kernel && $(MAKE) qemu

clean:
	cd coremod && $(MAKE) clean
	cd kernel && $(MAKE) clean