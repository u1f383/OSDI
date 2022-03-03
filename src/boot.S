.section ".text.boot"

.global _start

_start:
    # TODO: Disable interrupt
    # I use cpsid but it looks not work
    # cpsid

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
    b kernel

memzero:
    str xzr, [x0], #8
    cmp x0, x1
    b.ne memzero
    ret

slave:
    # Wait for interrupt event
    wfe
    b slave