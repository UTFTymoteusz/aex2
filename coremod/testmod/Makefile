MODULE_NAME  = testmod
MODULE_ORDER = 01

CC = g++
LD = ld

MKDIR = mkdir -p

BIN  := bin/
DEP_DEST := $(BIN)dep/
OBJ_DEST := $(BIN)obj/

CFILES    := $(shell find . -type f -name '*.cpp')
HFILES    := $(shell find . -type f -name '*.hpp')
OBJS      := $(patsubst %.o, $(OBJ_DEST)%.o, $(CFILES:.cpp=.cpp.o))

OBJS := $(OBJS)

MODULE_OBJ = $(BIN)$(MODULE_NAME).$(MODULE_ORDER).km

GFLAGS = -O2 -Wall -Wextra -nostdlib -pipe

INCLUDES := -I. -I$(ARCH) -Iinclude/ -I../../kernel/include \
		    -I../../kernel/arch/x64/include

CCFLAGS := $(GFLAGS)     \
	-lgcc                \
	-fno-rtti            \
	-fno-exceptions      \
	-ffreestanding       \
	-masm=intel          \
	-mcmodel=kernel      \
	-fno-pic             \
	-fno-stack-protector \
	-mno-red-zone        \
	$(INCLUDES)

ASFLAGS := -felf64

LDFLAGS := $(GFLAGS)        \
	-ffreestanding          \
	-z max-page-size=0x1000 \
	-no-pie

format:
	@$(MKDIR) $(BIN)
	clang-format -style=file -i ${CFILES} ${HFILES}

all: $(OBJS)
	@$(MKDIR) $(BIN)
	@$(LD) -r -o $(MODULE_OBJ) $(OBJS)

include $(shell find $(DEP_DEST) -type f -name *.d)

copy:
	@cp $(MODULE_OBJ) "$(COPY_DIR)"

clean:
	rm -rf $(BIN)

$(OBJ_DEST)%.cpp.o : %.cpp
	@$(MKDIR) ${@D}
	@$(MKDIR) $(dir $(DEP_DEST)$*)
	$(CC) $(CCFLAGS) -c $< -o $@ -MMD -MT $@ -MF $(DEP_DEST)$*.cpp.d