
CC = gcc
AS = nasm
AR = ar
RANLIB = ranlib
HOST_CC ?= cc

RELVERSION   := 0
SUBVERSION   := 0
PATCHVERSION := 2
EXTRAVERSION := -rc1

PRETTY_NAME := Cyber Diva

BUILD_DIR = build
KERNEL = $(BUILD_DIR)/kernel.bin
PLAIN_KERNEL = ./openkernel
OPENKERNEL_A = $(BUILD_DIR)/openkernel.a
LINUX_PROGRAM ?= $(BUILD_DIR)/openkernel
HOST_SOURCES = host/main.c

BUILD_NUMBER_FILE = .build_number

VERSION_DIR = include/generated
VERSION_HEADER = $(VERSION_DIR)/version.h
VERSION = $(RELVERSION).$(SUBVERSION).$(PATCHVERSION)$(EXTRAVERSION)

MOD_DIR = $(BUILD_DIR)/mod
MOD_SOURCES = $(wildcard modules/*.c)
MOD_OBJS = $(patsubst modules/%.c,$(MOD_DIR)/%.mo,$(MOD_SOURCES))

# Find all C and assembly sources.
C_SOURCES = $(shell find . -name "*.c" \
	-not -path "./$(BUILD_DIR)/*" \
	-not -path "./modules/*" \
	-not -path "./host/*" \
	-not -path "./bionicbox/*")

ASM_SOURCES = $(shell find . -name "*.asm" \
	-not -path "./$(BUILD_DIR)/*")

C_OBJECTS = $(patsubst ./%.c,$(BUILD_DIR)/c/%.o,$(C_SOURCES))
ASM_OBJECTS = $(patsubst ./%.asm,$(BUILD_DIR)/asm/%.o,$(ASM_SOURCES))

# kernel/entry.asm provides the Multiboot entry point.
ENTRY_ASM = kernel/entry.asm
ENTRY_OBJ = $(BUILD_DIR)/asm/kernel/entry.o

# The plain image does not include the Multiboot entry point.
PLAIN_ASM_OBJECTS = $(filter-out $(ENTRY_OBJ),$(ASM_OBJECTS))

LOGO_OBJ = $(BUILD_DIR)/logo_bmp.o

# Objects included in the archive.
PLAIN_OBJECTS = $(PLAIN_ASM_OBJECTS) $(C_OBJECTS) $(LOGO_OBJ) $(MOD_OBJS)

# Objects included in the bootable kernel.
BOOT_OBJECTS = $(ENTRY_OBJ) $(PLAIN_OBJECTS)

# Collect object directories for per-directory built-in.a archives.
C_OBJECT_DIRS = $(sort $(dir $(C_OBJECTS)))
ASM_OBJECT_DIRS = $(sort $(dir $(ASM_OBJECTS)))

C_BUILTIN_ARCHIVES = $(addsuffix built-in.a,$(C_OBJECT_DIRS))
ASM_BUILTIN_ARCHIVES = $(addsuffix built-in.a,$(ASM_OBJECT_DIRS))
BUILTIN_ARCHIVES = $(C_BUILTIN_ARCHIVES) $(ASM_BUILTIN_ARCHIVES)

# Automatic dependency files.
DEPS = $(C_OBJECTS:.o=.d) $(MOD_OBJS:.mo=.d)

CFLAGS = -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie -Iinclude -MMD -MP
LDFLAGS = -m32 -T linker.ld -ffreestanding -O2 -nostdlib -fno-pie -no-pie
PLAIN_LDFLAGS = -m32 -T linker.ld -ffreestanding -O2 -nostdlib -fno-pie -no-pie -Wl,-e,kernel_main
ASFLAGS = -f elf32
HOST_CFLAGS = -std=gnu99 -O2 -Wall -Wextra -Iinclude

.PHONY: all clean linux run-linux patch_modules

all: $(VERSION_HEADER) $(OPENKERNEL_A) $(PLAIN_KERNEL) $(KERNEL)

# Generate the kernel version header.
$(VERSION_HEADER):
	@mkdir -p $(VERSION_DIR)
	@echo "GEN $@"
	@printf '%s\n' \
		'#ifndef OPENKERNEL_VERSION_H' \
		'#define OPENKERNEL_VERSION_H' \
		'' \
		'#define OPENKERNEL_VERSION_MAJOR $(RELVERSION)' \
		'#define OPENKERNEL_VERSION_MINOR $(SUBVERSION)' \
		'#define OPENKERNEL_VERSION_PATCH $(PATCHVERSION)' \
		'#define OPENKERNEL_VERSION_EXTRA "$(EXTRAVERSION)"' \
		'#define OPENKERNEL_VERSION "$(VERSION)"' \
		'#define OPENKERNEL_PRETTYNAME "$(PRETTY_NAME)"' \
		'' \
		'#endif' > $@

# Compile C sources.
$(BUILD_DIR)/c/%.o: %.c $(VERSION_HEADER)
	@mkdir -p $(dir $@)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Assemble NASM sources.
$(BUILD_DIR)/asm/%.o: %.asm
	@mkdir -p $(dir $@)
	@echo "AS $<"
	@$(AS) $(ASFLAGS) $< -o $@

# Compile built-in module objects.
$(MOD_DIR)/%.mo: modules/%.c $(VERSION_HEADER)
	@mkdir -p $(dir $@)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Convert the bitmap logo into an ELF object.
$(LOGO_OBJ): kernel/video/logo.bmp
	@mkdir -p $(dir $@)
	@echo "OBJCOPY $<"
	@objcopy -I binary -O elf32-i386 -B i386 $< $@

# Generate a built-in.a archive for each C object directory.
define C_BUILTIN_RULE
$(1)built-in.a: $(filter $(1)%.o,$(C_OBJECTS))
	@rm -f "$$@"
	@echo "AR $$@"
	@$$(AR) rcsP "$$@" $$^
	@$$(RANLIB) "$$@"
endef

$(foreach dir,$(C_OBJECT_DIRS),$(eval $(call C_BUILTIN_RULE,$(dir))))

# Generate a built-in.a archive for each assembly object directory.
define ASM_BUILTIN_RULE
$(1)built-in.a: $(filter $(1)%.o,$(ASM_OBJECTS))
	@rm -f "$$@"
	@echo "AR $$@"
	@$$(AR) rcsP "$$@" $$^
	@$$(RANLIB) "$$@"
endef

$(foreach dir,$(ASM_OBJECT_DIRS),$(eval $(call ASM_BUILTIN_RULE,$(dir))))

# Combine all objects into openkernel.a.
# P preserves object paths inside the archive to avoid duplicate member names.
$(OPENKERNEL_A): $(PLAIN_OBJECTS) $(BUILTIN_ARCHIVES)
	@mkdir -p $(dir $@)
	@rm -f "$@"
	@echo "AR $@"
	@$(AR) rcsP "$@" $(PLAIN_OBJECTS)
	@$(RANLIB) "$@"

# Build the plain kernel image.
$(PLAIN_KERNEL): $(OPENKERNEL_A) linker.ld
	@echo "LD $@"
	@$(CC) $(PLAIN_LDFLAGS) -o "$@" $(OPENKERNEL_A)
	@test -f "$@"

# Build the bootable Multiboot kernel image.
$(KERNEL): $(ENTRY_OBJ) $(OPENKERNEL_A) linker.ld
	@mkdir -p $(dir $@)
	@echo "LD $@"
	@$(CC) $(LDFLAGS) -o "$@" $(ENTRY_OBJ) $(OPENKERNEL_A)
	@test -f "$@"
	@nm -n $(KERNEL) > Kernel.syms
	@if [ ! -f "$(BUILD_NUMBER_FILE)" ]; then echo 0 > "$(BUILD_NUMBER_FILE)"; fi
	@build=$$(cat "$(BUILD_NUMBER_FILE)"); \
	build=$$((build + 1)); \
	echo $$build > "$(BUILD_NUMBER_FILE)"; \
	echo "Kernel Image is ready! (build #$$build)"

# Build the host-side Linux program.
linux: $(KERNEL) $(LINUX_PROGRAM)

$(LINUX_PROGRAM): $(HOST_SOURCES)
	@mkdir -p $(dir $@)
	@echo "HOST CC $@"
	@$(HOST_CC) $(HOST_CFLAGS) -o $@ $(HOST_SOURCES)

# Build the ISO.
os.iso: $(KERNEL) $(shell find initfs -type f) iso/boot/grub/grub.cfg build_fs.sh copy_kernel.sh make_iso.sh
	@./copy_kernel.sh
	@./build_fs.sh
	@./make_iso.sh

run-linux: linux os.iso
	@./$(LINUX_PROGRAM) $(QEMU_ARGS)

# Report module sources.
patch_modules:
	@mkdir -p $(MOD_DIR)
	@echo "Checking modules in modules/..."
	@if [ -z "$(MOD_SOURCES)" ]; then \
		echo "No modules found in modules/."; \
	else \
		echo "Found module sources: $(MOD_SOURCES)"; \
	fi

# Remove generated build products.
clean:
	rm -rf $(BUILD_DIR) Kernel.syms openkernel $(VERSION_HEADER)

-include $(DEPS)