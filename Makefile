CC = gcc
AS = nasm

BUILD_DIR = build
KERNEL ?= $(BUILD_DIR)/kernel.bin

# Find all .c and .asm files recursively from the current directory
C_SOURCES = $(shell find . -name "*.c" -not -path "./$(BUILD_DIR)/*")
ASM_SOURCES = $(shell find . -name "*.asm" -not -path "./$(BUILD_DIR)/*")

C_OBJECTS = $(patsubst ./%.c, $(BUILD_DIR)/c/%.o, $(C_SOURCES))
ASM_OBJECTS = $(patsubst ./%.asm, $(BUILD_DIR)/asm/%.o, $(ASM_SOURCES))
OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

CFLAGS = -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie -Iinclude
LDFLAGS = -m32 -T linker.ld -ffreestanding -O2 -nostdlib -fno-pie
ASFLAGS = -f elf32

all: $(BUILD_DIR) $(KERNEL)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(KERNEL): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/c/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/asm/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean