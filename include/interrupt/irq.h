#ifndef _INTERRUPT_IRQ_H_
#define _INTERRUPT_IRQ_H_
#include <types.h>
#include <list.h>
#include <util.h>

#define TIME_UNIT 0x10000
// #define TIME_SLOT (0xa0000 / TIME_UNIT)
#define TIME_SLOT 0x1

#define INT_MAX_DEPTH 0x10
#define CORE0_INTERRUPT_SRC 0x40000060
#define CORE0_TIMER_IRQ_CTRL 0x40000040

#define TASK_MODE_LILO 0b0
#define TASK_MODE_FILO 0b1
#define TASK_MODE (TASK_MODE_LILO)

void irq_handler();

/* Enable / disable timer for Arm core0 timer IRQ */
static inline void enable_timer()
{
    *(uint32_t *) CORE0_TIMER_IRQ_CTRL = 2;
}

static inline void disable_timer()
{
    *(uint32_t *) CORE0_TIMER_IRQ_CTRL = 0;
}

static inline void update_timer()
{
    write_sysreg(cntp_tval_el0, TIME_UNIT);
    enable_timer();
}

static inline void disable_intr()
{
    __asm__("msr DAIFSet, 0xf");
}

static inline void enable_intr()
{
    __asm__("msr DAIFClr, 0xf");
}

#endif /* _KERNEL_IRQ_H_ */