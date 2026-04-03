EXTENSIONS ?= vx zvbb zvbc

# Default target
all:

# List of supported top-level targets
TARGETS := all clean qemu test

.PHONY: $(TARGETS) $(EXTENSIONS)

# For each target, iterate over all extension directories
$(TARGETS):
	@for dir in $(EXTENSIONS); do \
		echo "=== make -C $$dir $@ ==="; \
		$(MAKE) -C $$dir $@ || exit 1; \
	done
