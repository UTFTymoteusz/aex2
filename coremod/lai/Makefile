MODULE_NAME  = lai
MODULE_ORDER = 00

CXX ?= g++
CC  ?= gcc
LD   = ld

MKDIR = mkdir -p

BIN  := bin/
DEP_DEST := $(BIN)dep/
OBJ_DEST := $(BIN)obj/

LAI_CFILES := $(shell find ./lai/ -type f -name '*.c')
LAI_HFILES := $(shell find ./lai/ -type f -name '*.h')
LAI_OBJS   := $(patsubst %.o, $(OBJ_DEST)%.o, $(LAI_CFILES:.c=.c.o))

CXXFILES  := $(shell find . -type f -name '*.cpp')
HXXFILES  := $(shell find . -type f -name '*.hpp')
OBJS      := $(patsubst %.o, $(OBJ_DEST)%.o, $(CXXFILES:.cpp=.cpp.o))

OBJS := $(OBJS) $(LAI_OBJS)

MODULE_OBJ = $(BIN)$(MODULE_NAME).$(MODULE_ORDER).km

GFLAGS = -O3 -Wall -Wextra -nostdlib -pipe

LAI_INCLUDES := -I. -I./lai/include

INCLUDES := -I. -I$(ARCH) -Iinclude/ -I../../kernel/include \
		    -I../../kernel/arch/x64/include -I./lai/include

LAI_CCFLAGS := $(GFLAGS) \
	-std=gnu99           \
	-ffreestanding       \
	-masm=intel          \
	-mcmodel=kernel      \
	-fno-pic             \
	-fno-stack-protector \
	$(LAI_INCLUDES)

CXXFLAGS := $(GFLAGS)    \
	-std=c++17           \
	-fno-rtti            \
	-fno-exceptions      \
	-ffreestanding       \
	-masm=intel          \
	-mcmodel=kernel      \
	-fno-pic             \
	-fno-stack-protector \
	-mno-red-zone        \
	$(INCLUDES)

LDFLAGS := $(GFLAGS)        \
	-ffreestanding          \
	-z max-page-size=0x1000 \
	-no-pie

format:
	@$(MKDIR) $(BIN)
	clang-format -style=file -i ${CFILES} ${HFILES}

all: $(OBJS)
	echo $(OBJS)
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
	$(CXX) $(CXXFLAGS) -c $< -o $@ -MMD -MT $@ -MF $(DEP_DEST)$*.cpp.d

$(OBJ_DEST)%.c.o : %.c
	@$(MKDIR) ${@D}
	@$(MKDIR) $(dir $(DEP_DEST)$*)
	$(CC) $(LAI_CCFLAGS) -c $< -o $@ -MMD -MT $@ -MF $(DEP_DEST)$*.c.d