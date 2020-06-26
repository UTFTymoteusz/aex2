all:
	$(foreach dir, $(wildcard */), cd $(dir) && $(MAKE) all && cd ..;)
	$(foreach dir, $(wildcard */), cd $(dir) && $(MAKE) copy COPY_DIR="$(KERNELMOD_DIR)" && cd ..;)

clean:
	$(foreach dir, $(wildcard */), cd $(dir) && $(MAKE) clean && cd ..;)