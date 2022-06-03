.section ".text.boot"

.global _start

_start:
    ldr x1, =dtb_base
    str x0, [x1]

    # Kernel migration
    ldr x0, =_boot_base
    
    mov x1, x0
    ldr x2, =_start
    ldr x3, =_end

cp_boot:
    ldr x4, [x2], #8
    str x4, [x1], #8
    cmp x2, x3
    b.lt cp_boot

    ldr x1, =_start
    ldr x2, =_real_start
    sub x1, x2, x1
    add x0, x0, x1
    br x0

_real_start:    
    mrs x0, mpidr_el1
    # Get the core number
    and x0, x0, #0xff
    # Cpu is slave when core number != 0
    cbnz x0, slave

master:
    ldr x0, =_start
    # Set stack pointer
    mov sp, x0

    # Clean bss
    ldr x0, =__bss_start
    ldr x1, =__bss_end
    bl memzero
    b loader

memzero:
    str xzr, [x0], #8
    cmp x0, x1
    b.ne memzero
    ret

slave:
    # Wait for interrupt event
    wfe
    b slave

.section ".data"

.global dtb_base
dtb_base:
    .long 0