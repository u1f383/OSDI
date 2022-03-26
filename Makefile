CROSS_COMPILER_PREFIX ?= aarch64-unknown-linux-gnu

# -freestanding: standard library may not exist, and program ep may not be at "main"
# -mgeneral-regs-only: generate code which uses only the general-purpose registers,
#                      preventing the compiler from using fp and Advanced SIMD registers
#                      but will not impose any restrictions on the assembler
CFLAGS = -Wall \
		-mcpu=cortex-a53 \
		-march=armv8-a \
		-nostdlib \
		-nostartfiles \
		-ffreestanding \
		-mgeneral-regs-only \
		-g \
		-Iinclude

BUILD_DIR = build
BOOT_DIR = boot
KERNEL_DIR = kernel
SCRIPT_DIR = scripts

UBOOT_IMG = bootloader.img
UBOOT_ELF = bootloader.elf
BOOT_LINK_SCRIPT = $(SCRIPT_DIR)/linker_boot.ld

KERN_IMG = kernel8.img # The 8 of kernel8 means ARMv8
KERN_ELF = kernel8.elf
KERN_LINK_SCRIPT = $(SCRIPT_DIR)/linker_kern.ld

# stdio / pty
INTERFACE = stdio

all: $(KERN_IMG) $(UBOOT_IMG)

.PHONY: debug
debug_kern:
	qemu-system-aarch64 -M raspi3b \
						-cpu cortex-a53 \
						-kernel $(KERN_IMG) \
						-serial null \
						-serial $(INTERFACE) \
						-initrd initramfs.cpio \
						-dtb bcm2710-rpi-3-b-plus.dtb \
						-display none \
						-S -s

debug_boot:
	qemu-system-aarch64 -M raspi3b \
						-cpu cortex-a53 \
						-kernel $(UBOOT_IMG) \
						-serial null \
						-serial $(INTERFACE) \
						-initrd initramfs.cpio \
						-dtb bcm2710-rpi-3-b-plus.dtb \
						-display none \
						-S -s

.PHONY: run
run_kern:
	qemu-system-aarch64 -M raspi3b \
						-cpu cortex-a53 \
						-kernel $(KERN_IMG) \
						-serial null \
						-serial $(INTERFACE) \
						-initrd initramfs.cpio \
						-dtb bcm2710-rpi-3-b-plus.dtb \
						-display none

run_boot:
	qemu-system-aarch64 -M raspi3b \
						-cpu cortex-a53 \
						-kernel $(UBOOT_IMG) \
						-serial null \
						-serial $(INTERFACE) \
						-initrd initramfs.cpio \
						-dtb bcm2710-rpi-3-b-plus.dtb \
						-display none

ROOTFS = rootfs
.PHONY: new_cpio
new_cpio:
	make -C $(SCRIPT_DIR) user_program
	cp $(SCRIPT_DIR)/user_program $(ROOTFS)
	find $(ROOTFS)/ | cpio -o -H newc > initramfs.cpio

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(KERN_IMG) $(UBOOT_IMG)

UBOOT_SRC_FILES = $(shell find "./$(BOOT_DIR)" -name "*.c") $(shell find "./$(BOOT_DIR)" -name "*.S")
UBOOT_DEP_FILES = $(patsubst %,$(BUILD_DIR)/%.d,$(UBOOT_SRC_FILES))
UBOOT_OBJ_FILES = $(patsubst %,$(BUILD_DIR)/%.o,$(UBOOT_SRC_FILES))
-include UBOOT_DEP_FILES

KERN_SRC_FILES = $(shell find "./$(KERNEL_DIR)" -name "*.c") $(shell find "./$(KERNEL_DIR)" -name "*.S")
KERN_DEP_FILES = $(patsubst %,$(BUILD_DIR)/%.d,$(KERN_SRC_FILES))
KERN_OBJ_FILES = $(patsubst %,$(BUILD_DIR)/%.o,$(KERN_SRC_FILES))
-include KERN_DEP_FILES

LIB_SRC_FILES = $(shell find . -name "*.c"  ! -path "./$(BOOT_DIR)/*"    \
											! -path "./$(KERNEL_DIR)/*"  \
											! -path "./$(SCRIPT_DIR)/*"  )
LIB_DEP_FILES = $(patsubst %,$(BUILD_DIR)/%.d,$(LIB_SRC_FILES))
LIB_OBJ_FILES = $(patsubst %,$(BUILD_DIR)/%.o,$(LIB_SRC_FILES))
-include LIB_DEP_FILES

FILTER = $(foreach v,$(2),$(if $(findstring $(1),$(v)),$(v),))
BOOT_OBJ_FILES =  $(call FILTER,printf.c.o, $(LIB_OBJ_FILES))
BOOT_OBJ_FILES += $(call FILTER,uart.c.o, $(LIB_OBJ_FILES))
BOOT_OBJ_FILES += $(call FILTER,util.c.o, $(LIB_OBJ_FILES))
BOOT_OBJ_FILES += $(call FILTER,irq.c.o, $(LIB_OBJ_FILES))

test:
	@echo $(LIB_OBJ_FILES)

$(BUILD_DIR)/%.o: %
	mkdir -p $(@D)
	$(CROSS_COMPILER_PREFIX)-gcc $(CFLAGS) -MMD -c $< -o $@

$(KERN_IMG): $(LINK_SCRIPT) $(KERN_OBJ_FILES) $(LIB_OBJ_FILES)
	$(CROSS_COMPILER_PREFIX)-ld -T $(KERN_LINK_SCRIPT) -o $(BUILD_DIR)/$(KERN_ELF) $(KERN_OBJ_FILES) $(LIB_OBJ_FILES)
	$(CROSS_COMPILER_PREFIX)-objcopy $(BUILD_DIR)/$(KERN_ELF) -O binary $(KERN_IMG)

.PHONY: boot
boot: $(UBOOT_IMG)

$(UBOOT_IMG): $(LINK_SCRIPT) $(UBOOT_OBJ_FILES) $(BOOT_OBJ_FILES)
	$(CROSS_COMPILER_PREFIX)-ld -T $(BOOT_LINK_SCRIPT) -o $(BUILD_DIR)/$(UBOOT_ELF) $(UBOOT_OBJ_FILES) $(BOOT_OBJ_FILES)
	$(CROSS_COMPILER_PREFIX)-objcopy $(BUILD_DIR)/$(UBOOT_ELF) -O binary $(UBOOT_IMG)