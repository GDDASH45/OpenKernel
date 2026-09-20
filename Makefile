CC = gcc
AS = nasm

BUILD_DIR = build
KERNEL ?= $(BUILD_DIR)/kernel.bin

MOD_DIR = $(BUILD_DIR)/mod
MOD_SOURCES = $(wildcard modules/*.c)
MOD_OBJS = $(patsubst modules/%.c, $(MOD_DIR)/%.mo, $(MOD_SOURCES))

# Find all .c and .asm files, excluding build and modules directories
C_SOURCES = $(shell find . -name "*.c" -not -path "./$(BUILD_DIR)/*" -not -path "./modules/*")
ASM_SOURCES = $(shell find . -name "*.asm" -not -path "./$(BUILD_DIR)/*")

C_OBJECTS = $(patsubst ./%.c, $(BUILD_DIR)/c/%.o, $(C_SOURCES))
ASM_OBJECTS = $(patsubst ./%.asm, $(BUILD_DIR)/asm/%.o, $(ASM_SOURCES))



# Include the objcopy-generated logo object in the link list
LOGO_OBJ = $(BUILD_DIR)/logo_bmp.o
OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS) $(LOGO_OBJ)

# Automatic dependency files for header tracking
DEPS = $(C_OBJECTS:.o=.d) $(MOD_OBJS:.mo=.d)

CFLAGS = -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie -Iinclude -Ilib/lwext4/include -MMD -MP 
LDFLAGS = -m32 -T linker.ld -ffreestanding -O2 -nostdlib -fno-pie -no-pie
ASFLAGS = -f elf32

all: $(KERNEL)

$(KERNEL): $(OBJECTS) $(MOD_OBJS)
	@echo "LD $(KERNEL)"
	@$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(MOD_OBJS)
	nm -n $(KERNEL) > Kernel.syms
	@echo "kernel Image is ready!"

$(BUILD_DIR)/c/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/asm/%.o: %.asm
	@mkdir -p $(dir $@)
	@echo "AS $<"
	@$(AS) $(ASFLAGS) $< -o $@

$(MOD_DIR)/%.mo: modules/%.c
	@mkdir -p $(dir $@)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Rule to wrap logo.bmp into an ELF32 object file using objcopy
$(BUILD_DIR)/logo_bmp.o: kernel/video/logo.bmp
	@mkdir -p $(dir $@)
	@echo "OBJCOPY $<"
	@objcopy -I binary -O elf32-i386 -B i386 $< $@

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
	rm -rf $(BUILD_DIR) Kernel.syms

.PHONY: all clean patch_modules