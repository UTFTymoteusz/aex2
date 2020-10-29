format:
	clang-format -style=file -i $(shell find . -type f -name '*.cpp') $(shell find . -type f -name '*.hpp')
	
all:
	cd $(ARCH) && $(MAKE) all && cd ..

clean:
	$(foreach dir, $(wildcard */), cd $(dir) && $(MAKE) clean && cd ..;)