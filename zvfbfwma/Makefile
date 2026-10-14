xlen ?= 64
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

CFLAGS :=  -march=rv$(xlen)gcv_zvfbfmin_zvfbfwma -static -Os -ffunction-sections -fdata-sections -I../common -I../thirdparty -std=c++17
CXXFLAGS :=  -march=rv$(xlen)gcv_zvfbfmin_zvfbfwma -mabi=${abi} -static -Os -ffunction-sections -fdata-sections -I../common -I../thirdparty -std=c++17
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

# Baremetal code generation and build
# Usage: make baremetal RUNTIME=100 [CASE=vfwmaccbf16.vv]
RUNTIME ?= 100
BM_CROSS ?= riscv64-unknown-elf-
BM_CC    := $(BM_CROSS)gcc
BM_OBJDUMP := $(BM_CROSS)objdump
BM_CFLAGS  := -march=rv$(xlen)gcv_zvfbfmin_zvfbfwma -mabi=$(abi) -mcmodel=medany -O0 -g -ffreestanding -nostdlib -fno-common

BAREMETAL := $(patsubst %.cpp, %.baremetal, $(SRC_CPP))

ifdef CASE
BAREMETAL := $(CASE).baremetal
endif


BM_GEN := $(patsubst %.cpp, %.baremetal-gen, $(SRC_CPP))
BM_BUILD := $(patsubst %.cpp, %.baremetal-build, $(SRC_CPP))

ifdef CASE
BM_GEN := $(CASE).baremetal-gen
BM_BUILD := $(CASE).baremetal-build
endif

baremetal-generate: $(BM_GEN)

%.baremetal-gen: %.elf
	qemu-riscv$(xlen) $< --runtime $(RUNTIME) --baremetal

baremetal-compile: $(BM_BUILD)

%.baremetal-build: %.cpp.baremetal.c %.cpp.baremetal_entry.S %.cpp.baremetal.ld
	$(BM_CC) $(BM_CFLAGS) -c $*.cpp.baremetal_entry.S -o $*.cpp.baremetal_entry.o
	$(BM_CC) $(BM_CFLAGS) -c $*.cpp.baremetal.c -o $*.cpp.baremetal.o
	$(BM_CC) $(BM_CFLAGS) -nostartfiles -T $*.cpp.baremetal.ld -o $*.baremetal.elf \
		$*.cpp.baremetal_entry.o $*.cpp.baremetal.o
	$(BM_OBJDUMP) -S $*.baremetal.elf > $*.baremetal.asm
	@echo "[OK] $*.baremetal.elf built"

# Baremetal QEMU system-mode run
# Usage: make baremetal-run [CASE=vfwmaccbf16.vv]
QEMU_SYSTEM ?= qemu-system-riscv$(xlen)
QEMU_SYS_FLAGS := -M virt -cpu max -nographic -bios none

BM_ELF := $(patsubst %.cpp, %.baremetal.elf, $(SRC_CPP))
BM_RUN := $(patsubst %.cpp, %.baremetal-run, $(SRC_CPP))

ifdef CASE
BM_RUN := $(CASE).baremetal-run
endif

baremetal-run: $(BM_RUN)

%.baremetal-run: %.baremetal.elf
	@echo "[RUN] $<"
	@timeout 10 $(QEMU_SYSTEM) $(QEMU_SYS_FLAGS) -kernel $< -d int 2>$*.baremetal.int.log; \
	 ILLEGAL=$$(grep -c illegal_instruction $*.baremetal.int.log 2>/dev/null || echo 0); \
	 ECALL=$$(grep -c machine_ecall $*.baremetal.int.log 2>/dev/null || echo 0); \
	 echo "  illegal=$$ILLEGAL ecall=$$ECALL"; \
	 if [ "$$ECALL" -eq 0 ]; then echo "  [WARN] no ecall - might have hung"; fi

# Baremetal Sail run
# Usage: make baremetal-sail [CASE=xxx]
SAIL_SIM ?= sail_riscv_sim
SAIL_INST_LIMIT ?= 100000

BM_SAIL := $(patsubst %.cpp, %.baremetal-sail, $(SRC_CPP))

ifdef CASE
BM_SAIL := $(CASE).baremetal-sail
endif

baremetal-sail: $(BM_SAIL)

%.baremetal-sail: %.baremetal.elf
	@echo "[SAIL] $<"
	@OUT=$$($(SAIL_SIM) --inst-limit $(SAIL_INST_LIMIT) $< 2>&1); \
	 if echo "$$OUT" | grep -q "SUCCESS"; then echo "  [PASS] $<"; \
	 else echo "  [FAIL] $<"; echo "$$OUT"; fi

clean:
	rm -f *.o $(EXE_CPP) *.data *.bin
	rm -f *.baremetal.elf *.baremetal.asm *.baremetal.c *.baremetal.ld *.baremetal.mk *.baremetal_entry.S

cleandata:
	rm -f *.data

help:
	@echo "Available targets:"
	@echo "  all                - Build all Linux ELF binaries (default)"
	@echo "  qemu               - Run all tests in QEMU user mode"
	@echo "  test               - Run all tests on native RISC-V hardware"
	@echo "  baremetal-generate - Generate baremetal source files via QEMU user mode"
	@echo "  baremetal-compile  - Compile generated baremetal sources into ELFs"
	@echo "  baremetal-run      - Run baremetal ELFs in QEMU system mode (-cpu max)"
	@echo "  baremetal-sail     - Run baremetal ELFs in Sail RISC-V simulator"
	@echo "  clean              - Remove all generated files"
	@echo ""
	@echo "Options:"
	@echo "  CASE=<name>    - Target a single test case (without .cpp extension)"
	@echo "                   e.g. make baremetal-generate CASE=vfwmaccbf16.vv"
	@echo "  RUNTIME=<n>    - Number of random iterations for baremetal (default: 100)"
	@echo "  COMPILER=clang - Use clang instead of gcc"
	@echo "  DEBUG=1        - Build with -O0 -g for debugging"

.PHONY: help clean cleandata test baremetal-generate baremetal-compile baremetal-run baremetal-sail
