EXTENSIONS ?= vx vf vls zvbb zvbc zvkg zvkned zvksed zvknh zvksh

# Default target
all:

# List of supported top-level targets
TARGETS := all clean cleandata qemu test baremetal-generate baremetal-compile baremetal-run baremetal-sail

.PHONY: $(TARGETS) $(EXTENSIONS) help

# For each target, iterate over all extension directories
$(TARGETS):
	@for dir in $(EXTENSIONS); do \
		echo "=== make -C $$dir $@ ==="; \
		$(MAKE) -C $$dir $@ || exit 1; \
	done

help:
	@echo "Top-level Makefile - dispatches to extension directories"
	@echo ""
	@echo "Available targets:"
	@echo "  all                - Build all Linux ELF binaries (default)"
	@echo "  qemu               - Run all tests in QEMU user mode"
	@echo "  test               - Run all tests on native RISC-V hardware"
	@echo "  baremetal-generate - Generate baremetal source files via QEMU user mode"
	@echo "  baremetal-compile  - Compile generated baremetal sources into ELFs"
	@echo "  baremetal-run      - Run baremetal ELFs in QEMU system mode"
	@echo "  baremetal-sail     - Run baremetal ELFs in Sail RISC-V simulator"
	@echo "  clean              - Remove all generated files"
	@echo ""
	@echo "Options:"
	@echo "  EXTENSIONS=...  - Override extension list (default: vx vf vls zvbb zvbc)"
	@echo "  CASE=<name>     - Target a single test case (passed to sub-make)"
	@echo "  RUNTIME=<n>     - Iterations for baremetal generation (default: 100)"
	@echo ""
	@echo "Examples:"
	@echo "  make baremetal-generate EXTENSIONS=vx CASE=vadd.vv RUNTIME=10"
	@echo "  make baremetal-compile EXTENSIONS=vx"
	@echo "  make baremetal-run EXTENSIONS=vls"
