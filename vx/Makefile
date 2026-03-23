xlen := 64
abi := lp64d
ifeq ($(xlen),32)
	abi := ilp32d
endif

UNAME_M := $(shell uname -m)

CC := riscv64-unknown-linux-gnu-gcc
CXX := riscv64-unknown-linux-gnu-g++

CFLAGS :=  -march=rv64gcv -static -g -I../common -I../thirdparty -std=c++17
CXXFLAGS :=  -march=rv64gcv -mabi=${abi} -static -g -I../common -I../thirdparty -std=c++17


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
		$(CXX) $(CXXFLAGS) -o $@ $<

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
