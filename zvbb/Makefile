xlen := 64
abi := lp64d
ifeq ($(xlen),32)
	abi := ilp32d
endif
UNAME_M := $(shell uname -m)

COMPILER ?= gcc

ifeq ($(COMPILER),clang)
CC := riscv64-unknown-linux-gnu-clang
CXX := riscv64-unknown-linux-gnu-clang++
else
CC := riscv64-unknown-linux-gnu-gcc
CXX := riscv64-unknown-linux-gnu-g++
endif


CFLAGS :=  -march=rv64gcv_zvbb -static -Os -ffunction-sections -fdata-sections -I../common -I../thirdparty -std=c++17
CXXFLAGS := -march=rv64gcv_zvbb -mabi=${abi} -static -Os -ffunction-sections -fdata-sections -I../common -I../thirdparty -std=c++17
LDFLAGS := -Wl,--gc-sections

ifdef DEBUG
CFLAGS += -O0 -g
CXXFLAGS += -O0 -g
endif

SRC_C := $(wildcard *.c)
SRC_CPP := $(wildcard *.cpp)

OBJ_C := $(patsubst %.c,%.o,$(SRC_C))
OBJ_CPP := $(patsubst %.cpp,%.o,$(SRC_CPP))

EXE_C := $(patsubst %.c,%.elf,$(SRC_C))
EXE_CPP := $(patsubst %.cpp,%.elf,$(SRC_CPP))
QEMU := $(patsubst %.cpp, %.qemu, $(SRC_CPP))
TEST := $(patsubst %.cpp, %.test, $(SRC_CPP))

all: $(EXE_CPP)

%.elf: %.cpp
		$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $<

qemu: $(EXE_CPP) $(QEMU)

%.qemu:%.elf
	qemu-riscv${xlen} $<

test: $(EXE_CPP) $(TEST)

%.test:%.elf
ifneq ($(findstring riscv,$(UNAME_M)),)
	./$<
else
	$(error Please run the tests on a RISC-V platform)
endif

clean:
	rm -f *.o $(EXE_CPP) *.data *.bin

.PHONY: clean test
