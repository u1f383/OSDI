
.section ".text.kern"

.global _kernel, \
        ret_from_fork

_kernel:
    # Temporary move dtb base address to x15
    mov x15, x0
    
    bl from_el2_to_el1

#define TCR_CONFIG_REGION_48bit (((64 - 48) << 0) | ((64 - 48) << 16))
# 4KB granule with 48 bits:
#  L0 [47:39] (9 bits)
#  L1 [38:30] (9 bits)
#  L2 [29:21] (9 bits)
#  L3 [20:12] (9 bits)
#  offset [11:0] (12 bits)
#define TCR_CONFIG_4KB ((0b00 << 14) |  (0b10 << 30))
#define TCR_CONFIG_DEFAULT (TCR_CONFIG_REGION_48bit | TCR_CONFIG_4KB)

#define MAIR_DEVICE_nGnRnE 0b00000000
#define MAIR_NORMAL_NOCACHE 0b01000100
#define MAIR_IDX_DEVICE_nGnRnE 0
#define MAIR_IDX_NORMAL_NOCACHE 1

#define PD_TABLE 0b11
#define PD_BLOCK 0b01
#define PD_TBENT 0b11

#define AF_ACCESS (1 << 10)
# | AF (access flag, bit[10]) | AttrIndx (bit[4:2]) | Table descriptor type[1:0]
#define BOOT_PMD_RAM_ATTR   (AF_ACCESS | (MAIR_IDX_NORMAL_NOCACHE << 2) | PD_BLOCK)
#define BOOT_PMD_PERIF_ATTR (AF_ACCESS | (MAIR_IDX_DEVICE_nGnRnE << 2) | PD_BLOCK)
#define PERIF_BOOT_PUD_ATTR (AF_ACCESS | (MAIR_IDX_DEVICE_nGnRnE << 2) | PD_BLOCK)

page_table_setup:
    # Set TG0 to 4KB (0b00) and T0SZ to 16
    ldr x0, =TCR_CONFIG_DEFAULT
    msr tcr_el1, x0

    # Setup MAIR
    ldr x0, =( \
    (MAIR_DEVICE_nGnRnE << (MAIR_IDX_DEVICE_nGnRnE * 8)) | \
    (MAIR_NORMAL_NOCACHE << (MAIR_IDX_NORMAL_NOCACHE * 8)) \
    )
    msr mair_el1, x0

    # PGD's page frame at 0x0
    mov x0, 0
    # 1st PGD entry
    mov x1, 0x1000
    ldr x2, =PD_TABLE
    orr x2, x1, x2
    str x2, [x0]

    # 1st PUD entry
    mov x3, 0x2000
    ldr x2, =PD_TABLE
    orr x3, x2, x3
    str x3, [x1]

    # 2nd PUD entry
    ldr x2, =PERIF_BOOT_PUD_ATTR
    mov x3, 0x40000000
    orr x3, x2, x3
    str x3, [x1, #8]

    # setup PMD
    mov x1, 0x2000
    ldr x2, =BOOT_PMD_RAM_ATTR
    mov x3, 0x00000000
    mov x4, 0x3B400000
    orr x3, x2, x3

setup_ram_page:
    str x3, [x1], #8
    add x3, x3, 0x200000
    cmp x3, x4
    b.lt setup_ram_page
    
    ldr x2, =BOOT_PMD_PERIF_ATTR
    mov x3, 0x3B400000
    mov x4, 0x40000000
    orr x3, x2, x3

setup_perif_page:
    str x3, [x1], #8
    add x3, x3, 0x200000
    cmp x3, x4
    b.lt setup_perif_page

    # Load PGD to the bottom translation-based register
    msr ttbr0_el1, x0
    # Also load PGD to the upper translation based register
    msr ttbr1_el1, x0

    # MMU enable for EL1 and EL0
    mrs x2, sctlr_el1
    orr x2, x2, 1
    msr sctlr_el1, x2

boot_rest:
    # Setup exception vector table
    ldr x0, =exception_handler_table
    msr vbar_el1, x0

    # Enable timer for ARM
    mov x0, 1
    msr cntp_ctl_el0, x0

    # Run kernel
    mov x0, x15
    ldr x2, =kernel
    br x2

from_el2_to_el1:
    # Allow select stack
    mov x0, 1
    msr spsel, x0

    # Setup kernel stack
    ldr x0, =_stack
    msr sp_el1, x0

    # Mark EL1 is AArch64 in hypervisor control register
    mov x0, (1 << 31)
    mrs x1, hcr_el2
    orr x0, x0, x1
    msr hcr_el2, x0
    
    # Set return exception level and enable all interrupt
    mov x0, 0x5
    msr spsr_el2, x0

    # Set exception return address to the value of link register
    msr elr_el2, lr
    eret

# Save general registers to stack
.macro save_registers
    sub sp, sp, 16 * 17
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

    mrs x0, elr_el1
    mrs x1, spsr_el1
    mrs x2, sp_el0
    
    stp x30, x0, [sp, 16 * 15]
    stp x1, x2, [sp, 16 * 16]
.endm

# Load general registers from stack
.macro restore_registers
    ldp x30, x0, [sp, 16 * 15]
    ldp x1, x2, [sp, 16 * 16]

    msr elr_el1, x0
    msr spsr_el1, x1
    msr sp_el0, x2
    
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
    add sp, sp, 16 * 17
.endm

# .align 7 means 2^7 alignment
.balign 0x800
exception_handler_table:
    # 0
dummy_handler0:
    bl dummy_handler0
.align 7
    # 1
dummy_handler1:
    bl dummy_handler1
.align 7
    # 2
dummy_handler2:
    bl dummy_handler2
.align 7
    # 3
dummy_handler3:
    bl dummy_handler3

# -------------------------
.align 7
    # 4
    b sync_exception_handler
.align 7
    # 5 - SP_ELx - IRQ
    b irq_handler_wrapper
.align 7
    # 6
dummy_handler6:
    bl dummy_handler6
.align 7
    # 7
dummy_handler7:
    bl dummy_handler7

# -------------------------
.align 7
    # 8 - Lower exception level - synchronous event
    b sync_exception_handler
.align 7
    # 9 - Lower exception level - IRQ
    b irq_handler_wrapper
.align 7
    # 10
dummy_handler10:
    bl dummy_handler10
.align 7
    # 11
dummy_handler11:
    bl dummy_handler11

# -------------------------
.align 7
    # 12
dummy_handler12:
    bl dummy_handler12
.align 7
    # 13
dummy_handler13:
    bl dummy_handler13
.align 7
    # 14
dummy_handler14:
    bl dummy_handler14
.align 7
    # 15
dummy_handler15:
    bl dummy_handler15

.align 7
dummy_handler_exception:
    b dummy_handler_exception

sync_exception_handler:
    save_registers

    # Get exception class (EC)
    mrs x0, esr_el1
    asr x1, x0, 26
    
    # SVC
    cmp x1, 0x15
    b.eq svc_handler
    cmp x1, 0x20
    b.eq page_fault_handler
    cmp x1, 0x21
    b.eq page_fault_handler
    cmp x1, 0x24
    b.eq page_fault_handler
    cmp x1, 0x25
    b.eq page_fault_handler
    b dummy_handler_exception

page_fault_handler:
    mrs x0, far_el1
    mrs x1, esr_el1
    bl do_page_fault
    ldr x0, [sp ,16 * 0]
    b sync_exception_handler_end

svc_handler:
    # Get syscall number
    mov x0, x8
    bl sysno_check
    cmp x0, 0
    b.ne sync_exception_handler_end

    # load argument
    ldp x0, x1, [sp ,16 * 0]
    ldp x2, x3, [sp ,16 * 1]
    ldp x4, x5, [sp ,16 * 2]

    # Get the correspoding svc handler
    ldr x7, =svc_table
    ldr x9, [x7, x8, lsl#3]
    
    # Trick to get trap frame
    mov x8, sp

    # Enable interrupt
    msr DAIFClr, 0xf
    blr x9
    msr DAIFSet, 0xf

sync_exception_handler_end:
    ldp x30, x1, [sp, 16 * 15]
    ldp x2, x3, [sp, 16 * 16]

    msr elr_el1, x1
    msr spsr_el1, x2
    msr sp_el0, x3
    
    # Need not load x0
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
    eret

irq_handler_wrapper:
    save_registers
    bl irq_handler
    bl try_schedule

    mov x0, sp
    bl try_signal_handle

    msr DAIFSet, 0xf
    restore_registers
    qq:
    eret
