
.section ".text.kern"

.global _kernel, \
        from_el2_to_el1, \
        from_el1_to_el0, \
        uart_intr_handler, \
        irq_handler, \
        timer_intr_handler

_kernel:
    ldr x1, =dtb_base
    str x0, [x1]
    
    bl from_el2_to_el1

    # Setup exception vector table
    ldr x0, =exception_handler_table
    msr vbar_el1, x0

    # Setup kernel stack
    ldr x0, =_stack
    mov sp, x0

    # Enable timer for ARM
    mov x0, 1
    msr cntp_ctl_el0, x0

    # Run kernel
    b kernel

from_el2_to_el1:
    # Allow select stack
    mov x0, 1
    msr spsel, x0

    mov x0, 0x1000000
    msr sp_el1, x0

    mov x0, (1 << 31)
    mrs x1, hcr_el2

    # Mark EL1 is AArch64 in hypervisor control register
    orr x0, x0, x1
    msr hcr_el2, x0
    
    #        1           1        1        1       0 0    0101
    #     (Debug)     (SError)  (IRQ)     (FIQ)          (Mode)
    # Debug-exc-mask  Exc-mask Exc-mask Exc-mask   X X   (EL1h)
    # Setup saved program status register
    # and enable all interrupt (disable: 0x3c5)
    mov x0, 0x5
    msr spsr_el2, x0

    # Set return address in exception link register
    # to the value of link registe
    msr elr_el2, lr
    eret

from_el1_to_el0:
    # Return to user program
    msr elr_el1, x0
    
    # Setup user program stack pointer
    mov x0, 0x10000000
    msr sp_el0, x0

    # Set exception mode to EL0t
    # and enable all interrupt (disable: 0x3c0)
    mov x0, 0x0
    msr spsr_el1, x0

    eret

# Save general registers to stack
.macro save_greg
    sub sp, sp, 32 * 8
    stp x0, x1, [sp ,16 * 0]
    stp x2, x3, [sp ,16 * 1]
    stp x4, x5, [sp ,16 * 2]
    stp x6, x7, [sp ,16 * 3]
    stp x8, x9, [sp ,16 * 4]
    stp x10, x11, [sp ,16 * 5]
    stp x12, x13, [sp ,16 * 6]
    stp x14, x15, [sp ,16 * 7]
    stp x16, x17, [sp ,16 * 8]
    stp x18, x19, [sp ,16 * 9]
    stp x20, x21, [sp ,16 * 10]
    stp x22, x23, [sp ,16 * 11]
    stp x24, x25, [sp ,16 * 12]
    stp x26, x27, [sp ,16 * 13]
    stp x28, x29, [sp ,16 * 14]
    str x30, [sp, 16 * 15]
.endm

.macro save_intr_reg
    sub sp, sp, 4 * 8
    mrs x0, elr_el1
    mrs x1, spsr_el1
    stp x0, x1, [sp ,16 * 15 + 8]
.endm

# Load general registers from stack
.macro load_greg
    ldp x0, x1, [sp ,16 * 0]
    ldp x2, x3, [sp ,16 * 1]
    ldp x4, x5, [sp ,16 * 2]
    ldp x6, x7, [sp ,16 * 3]
    ldp x8, x9, [sp ,16 * 4]
    ldp x10, x11, [sp ,16 * 5]
    ldp x12, x13, [sp ,16 * 6]
    ldp x14, x15, [sp ,16 * 7]
    ldp x16, x17, [sp ,16 * 8]
    ldp x18, x19, [sp ,16 * 9]
    ldp x20, x21, [sp ,16 * 10]
    ldp x22, x23, [sp ,16 * 11]
    ldp x24, x25, [sp ,16 * 12]
    ldp x26, x27, [sp ,16 * 13]
    ldp x28, x29, [sp ,16 * 14]
    ldr x30, [sp, 16 * 15]
    add sp, sp, 32 * 8
.endm

.macro load_intr_reg
    ldp x0, x1, [sp ,16 * 15 + 8]
    msr elr_el1, x0
    msr spsr_el1, x1
    add sp, sp, 4 * 8
.endm

# .align 7 means 2^7 alignment
.balign 0x800
exception_handler_table:
    # 0
    bl dummy_handler
.align 7
    # 1
    bl dummy_handler
.align 7
    # 2
    bl dummy_handler
.align 7
    # 3
    bl dummy_handler
.align 7
    # 4
    bl dummy_handler
.align 7
    # 5 - SP_ELx - IRQ
    b _irq_handler_wrapper

.align 7
    # 6
    bl dummy_handler
.align 7
    # 7
    bl dummy_handler

.align 7
    save_greg
    # 8 - Lower exception level - synchronous event
    mrs x0, esr_el1

    # Get exception class (EC)
    asr x1, x0, 26

    # If SVC
    cmp x1, 0x15
    b.eq svc_handler

    # Do nothing
    bl dummy_handler

.align 7
    # 9 - Lower exception level - IRQ
.align 7
    # 10
    bl dummy_handler
.align 7
    # 11
    bl dummy_handler

.align 7
    # 12
    bl dummy_handler
.align 7
    # 13
    bl dummy_handler
.align 7
    # 14
    bl dummy_handler
.align 7
    # 15
    bl dummy_handler
.align 7

dummy_handler:
    b dummy_handler

#define SVC_NUM 0x10
svc_handler:
    # Get instruction specific syndrome (ISS)
    and x1, x0, ((1 << 24) - 1)
    cmp x1, SVC_NUM
    b.ge dummy_handler

    ldr x2, =svc_table

    # Get the correspoding svc handler
    ldr x0, [x2, x1, lsl#3]
    blr x0    

    load_greg
    eret

_irq_handler_wrapper:
    save_greg
    save_intr_reg

    bl irq_handler

    msr DAIFSet, 0xf
    load_intr_reg
    load_greg
    
    eret

.section ".data"

.global dtb_base
dtb_base:
    .long 0