MODULE_NAME = testmod

CC = g++
AS = nasm
LD = ld

MKDIR = mkdir -p

BIN  := bin/
BOOT := boot/
DEP_DEST := $(BIN)dep/
OBJ_DEST := $(BIN)obj/

CFILES    := $(shell find . -type f -name '*.cpp')
HFILES    := $(shell find . -type f -name '*.hpp')
ASMFILES  := $(shell find . -type f -name '*.asm')
OBJS      := $(patsubst %.o, $(OBJ_DEST)%.o, $(CFILES:.cpp=.cpp.o) $(ASMFILES:.asm=.asm.o))

OBJS := $(OBJS)

ARCH = arch/x64/

GFLAGS = -O2 -Wall -Wextra -nostdlib -pipe

INCLUDES := -I. -I$(ARCH) -Iinclude/ -I../../kernel/include

CCFLAGS := $(GFLAGS) \
	-lgcc      \
	-fno-rtti  \
	-fno-exceptions \
	-ffreestanding  \
	-masm=intel     \
	-mcmodel=kernel \
	-mno-red-zone   \
	-fno-pic        \
	-fno-stack-protector    \
	-fno-omit-frame-pointer \
	$(INCLUDES)

ASFLAGS := -felf64

LDFLAGS := $(GFLAGS) \
	-ffreestanding \
	-z max-page-size=0x1000 \
	-no-pie

format:
	@$(MKDIR) $(BIN)
	clang-format -style=file -i ${CFILES} ${HFILES}

all: $(OBJS)
	@$(MKDIR) $(BIN)
	@$(LD) -r -o $(BIN)$(MODULE_NAME).km $(OBJS)

include $(shell find $(DEP_DEST) -type f -name *.d)

copy:
	@cp $(BIN)$(MODULE_NAME).km "$(COPY_DIR)"

clean:
	rm -rf $(DEP_DEST)	
	rm -rf $(OBJ_DEST)	

$(OBJ_DEST)%.cpp.o : %.cpp
	@$(MKDIR) ${@D}
	@$(MKDIR) $(dir $(DEP_DEST)$*)
	$(CC) $(CCFLAGS) -c $< -o $@ -MMD -MT $@ -MF $(DEP_DEST)$*.cpp.d

$(OBJ_DEST)%.asm.o : %.asm
	@$(MKDIR) ${@D}
	$(AS) $(ASFLAGS) $< -o $@