format:
	@$(MKDIR) $(BIN)
	clang-format -style=file -i ${CFILES} ${HFILES}
	
all:
	cd testmod && $(MAKE) all
	cd testmod && $(MAKE) copy COPY_DIR="$(COPY_DIR)"

clean:
	cd testmod && $(MAKE) clean