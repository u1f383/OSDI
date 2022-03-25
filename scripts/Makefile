CROSS_COMPILER_PREFIX ?= aarch64-unknown-linux-gnu

CFLAGS = -Wall \
		-mcpu=cortex-a53 \
		-march=armv8-a \
		-nostdlib \
		-nostartfiles \
		-ffreestanding \
		-mgeneral-regs-only \
		-g

USER_LINK_SCRIPT = ./linker_user.ld

user_program: user_program.S $(USER_LINK_SCRIPT)
	$(CROSS_COMPILER_PREFIX)-gcc $(CFLAGS) -c $< -o $@.o
	$(CROSS_COMPILER_PREFIX)-ld -T $(USER_LINK_SCRIPT) -o $@ $@.o
	$(CROSS_COMPILER_PREFIX)-objcopy $@ -O binary $@

dtb_parser: dtb_parser.c
	gcc -o $@ $<

clean:
	rm -rf dtb_parser user_program