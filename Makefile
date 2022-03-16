CROSS_COMPILER_PREFIX ?= aarch64-unknown-linux-gnu

# -freestanding: standard library may not exist, and program ep may not be at "main"
# -mgeneral-regs-only: generate code which uses only the general-purpose registers,
#                      preventing the compiler from using fp and Advanced SIMD registers
#                      but will not impose any restrictions on the assembler
CFLAGS = -Wall \
		-nostdlib \
		-nostartfiles \
		-ffreestanding \
		-mgeneral-regs-only \
		-Iinclude

BUILD_DIR = build
UBOOT = bootloader.img
ELF = kernel8.elf
IMG = kernel8.img # The 8 of kernel8 means ARMv8
LINK_SCRIPT = scripts/linker.ld


all: $(IMG)

.PHONY: debug
debug:
	qemu-system-aarch64 -M raspi3b \
						-kernel $(IMG) \
						-serial null \
						-serial stdio \
						-display none \
						-S -s

.PHONY: run
run:
	qemu-system-aarch64 -M raspi3b \
						-kernel $(IMG) \
						-serial null \
						-serial stdio \
						-display none

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(IMG)

SRC_FILES = $(shell find . -name "*.c") $(shell find . -name "*.S")
DEP_FILES = $(patsubst %,$(BUILD_DIR)/%.d,$(SRC_FILES))
OBJ_FILES = $(patsubst %,$(BUILD_DIR)/%.o,$(SRC_FILES))

-include DEP_FILES

$(BUILD_DIR)/%.o: %
	mkdir -p $(@D)
	$(CROSS_COMPILER_PREFIX)-gcc $(CFLAGS) -MMD -c $< -o $@

$(IMG): $(LINK_SCRIPT) $(OBJ_FILES)
	$(CROSS_COMPILER_PREFIX)-ld -T $(LINK_SCRIPT) -o $(BUILD_DIR)/$(ELF) $(OBJ_FILES)
	$(CROSS_COMPILER_PREFIX)-objcopy $(BUILD_DIR)/$(ELF) -O binary $(IMG)