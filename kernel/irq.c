#include <kernel/irq.h>
#include <gpio/uart.h>
#include <types.h>
#include <util.h>

IntState int_stack[ INT_MAX_DEPTH ];
int32_t int_stack_top = 0;

void irq_handler()
{
    uint32_t int_src = *(uint32_t *) CORE0_INTERRUPT_SRC;
    // int_stack[int_stack_top++] = (IntState) { .elr_el1  = read_sysreg(elr_el1),
    //                                           .spsr_el1 = read_sysreg(spsr_el1) };

    /* Timer handling - check if CNTPNSIRQ interrupt bit is set */
    if (int_src & 0b10)
    {
        uint32_t frq = read_sysreg(cntfrq_el0);
        write_sysreg(cntp_tval_el0, frq*2);
    }
    else
        uart_int_handler();
}

