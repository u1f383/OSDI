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
SRC_DIR = src
ELF = kernel8.elf
IMG = kernel8.img # the 8 of kernel8 means ARMv8
LINK_SCRIPT = linker.ld

all: $(IMG)

debug:
	qemu-system-aarch64 -M raspi3b \
						-kernel $(IMG) \
						-serial null \
						-serial stdio \
						-display none \
						-S -s

run:
	qemu-system-aarch64 -M raspi3b \
						-kernel $(IMG) \
						-serial null \
						-serial stdio \
						-display none

clean:
	rm -rf $(BUILD_DIR) $(IMG)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(@D)
	$(CROSS_COMPILER_PREFIX)-gcc $(CFLAGS) -MMD -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S
	mkdir -p $(@D)
	$(CROSS_COMPILER_PREFIX)-gcc $(CFLAGS) -MMD -c $< -o $@

C_FILES = $(wildcard $(SRC_DIR)/**/*.c $(SRC_DIR)/*.c)
ASM_FILES = $(wildcard $(SRC_DIR)/**/*.S $(SRC_DIR)/*.S)
OBJ_FILES = $(C_FILES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
OBJ_FILES += $(ASM_FILES:$(SRC_DIR)/%.S=$(BUILD_DIR)/%.o)


$(IMG): $(SRC_DIR)/$(LINK_SCRIPT) $(OBJ_FILES)
	$(CROSS_COMPILER_PREFIX)-ld -T $(SRC_DIR)/$(LINK_SCRIPT) -o $(BUILD_DIR)/$(ELF) $(OBJ_FILES)
	$(CROSS_COMPILER_PREFIX)-objcopy $(BUILD_DIR)/$(ELF) -O binary $(IMG)