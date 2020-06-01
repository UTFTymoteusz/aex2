format:
	clang-format -style=file -i ${CFILES} ${HFILES}
	
all:
	$(foreach dir, $(wildcard */), cd $(dir) && $(MAKE) all && cd ..;)
	$(foreach dir, $(wildcard */), cd $(dir) && $(MAKE) copy COPY_DIR="$(COPY_DIR)" && cd ..;)

clean:
	$(foreach dir, $(wildcard */), cd $(dir) && $(MAKE) clean && cd ..;)