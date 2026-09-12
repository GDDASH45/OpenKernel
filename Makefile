CC = gcc
AS = nasm

BUILD_DIR = build
KERNEL ?= $(BUILD_DIR)/kernel.bin

MOD_DIR = $(BUILD_DIR)/mod
MOD_SOURCES = $(wildcard modules/*.c)
MOD_OBJS = $(patsubst modules/%.c, $(MOD_DIR)/%.o, $(MOD_SOURCES))

# Find all .c and .asm files, excluding build and modules directories
C_SOURCES = $(shell find . -name "*.c" -not -path "./$(BUILD_DIR)/*" -not -path "./modules/*")
ASM_SOURCES = $(shell find . -name "*.asm" -not -path "./$(BUILD_DIR)/*")

C_OBJECTS = $(patsubst ./%.c, $(BUILD_DIR)/c/%.o, $(C_SOURCES))
ASM_OBJECTS = $(patsubst ./%.asm, $(BUILD_DIR)/asm/%.o, $(ASM_SOURCES))
OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# Automatic dependency files for header tracking
DEPS = $(C_OBJECTS:.o=.d) $(MOD_OBJS:.o=.d)

CFLAGS = -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie -Iinclude -MMD -MP
LDFLAGS = -m32 -T linker.ld -ffreestanding -O2 -nostdlib -fno-pie -no-pie
ASFLAGS = -f elf32

all: $(KERNEL)

$(KERNEL): $(OBJECTS) $(MOD_OBJS)
	@echo "LD $(KERNEL)"
	@$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(MOD_OBJS)
	@echo "kernel Image is ready!"

$(BUILD_DIR)/c/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/asm/%.o: %.asm
	@mkdir -p $(dir $@)
	@echo "AS $<"
	@$(AS) $(ASFLAGS) $< -o $@

$(MOD_DIR)/%.o: modules/%.c
	@mkdir -p $(dir $@)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Include dependency files safely
-include $(DEPS)

patch_modules:
	@mkdir -p $(MOD_DIR)
	@echo "Checking modules in modules/..."
	@if [ -z "$(MOD_SOURCES)" ]; then \
		echo "No modules found in modules/. Add .c files there to compile them as modules."; \
	else \
		echo "Found module sources: $(MOD_SOURCES)"; \
	fi

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean patch_modules