#ifndef _IRQ_H_
#define _IRQ_H_

#include <types.h>
#include <list.h>
#include <util.h>

#define TIME_UNIT 0x1000
#define TIME_SLOT 0x1

#define INT_MAX_DEPTH 0x10
#define CORE0_INTERRUPT_SRC  0xffff000040000060
#define CORE0_TIMER_IRQ_CTRL 0xffff000040000040

#define TASK_MODE_LILO 0b0
#define TASK_MODE_FILO 0b1
#define TASK_MODE (TASK_MODE_LILO)

/* Enable / disable timer for Arm core0 timer IRQ */
#define enable_timer() do { \
    *(uint32_t *)CORE0_TIMER_IRQ_CTRL = 2; } while (0)

#define disable_timer() do { \
    *(uint32_t *)CORE0_TIMER_IRQ_CTRL = 0; } while (0)

#define disable_intr() do { __asm__("msr DAIFSet, 0xf"); } while (0)
#define enable_intr() do { __asm__("msr DAIFClr, 0xf"); } while (0)

void irq_handler();

#endif /* _IRQ_H_ */