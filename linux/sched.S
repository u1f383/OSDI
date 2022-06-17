.global switch_to
switch_to:
    stp x19, x20, [x0, 16 * 0]
    stp x21, x22, [x0, 16 * 1]
    stp x23, x24, [x0, 16 * 2]
    stp x25, x26, [x0, 16 * 3]
    stp x27, x28, [x0, 16 * 4]
    stp fp, lr, [x0, 16 * 5]
    mov x9, sp
    str x9, [x0, 16 * 6]

    ldp x19, x20, [x1, 16 * 0]
    ldp x21, x22, [x1, 16 * 1]
    ldp x23, x24, [x1, 16 * 2]
    ldp x25, x26, [x1, 16 * 3]
    ldp x27, x28, [x1, 16 * 4]
    ldp fp, lr, [x1, 16 * 5]
    ldr x9, [x1, 16 * 6]
    mov sp,  x9
    msr tpidr_el1, x1

    # Ensure write has completed
    dsb ish
    # Switch translation based address
    msr ttbr0_el1, x2
    # Invalidate all TLB entries
    tlbi vmalle1is
    # Ensure completion of TLB invalidatation
    dsb ish
    # Clear pipeline
    isb

    # Enable interrupt
    msr DAIFClr, 0xf
    ret

.global get_current
get_current:
    mrs x0, tpidr_el1
    ret

.global from_el1_to_el0
from_el1_to_el0:
    # Temperary disable interrupt
    msr DAIFSet, 0xf

    # Return to user program
    msr elr_el1, x19
    
    # Setup user program stack pointer
    msr sp_el0, x20

    # Set exception mode to EL0
    # and enable all interrupt (disable: 0x3c0)
    mov x0, 0x0
    msr spsr_el1, x0

    eret

.global __thread_trampoline
__thread_trampoline:
    mov x0, x19
    mov x1, x20
    b thread_trampoline

.global fork_trampoline
fork_trampoline:
    # Temperary disable interrupt
    msr DAIFSet, 0xf
    
    msr elr_el1, x19
    msr sp_el0, x20

    ldr x1, [sp ,16 * 0 + 8]
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
    add sp, sp, 16 * 17

    mov x0, 0x0
    msr spsr_el1, x0
    eret

.global call_sigreturn
call_sigreturn:
    mov x8, 0x15
    svc 0