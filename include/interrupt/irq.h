#ifndef _INTERRUPT_IRQ_H_
#define _INTERRUPT_IRQ_H_
#include <types.h>

#define INT_MAX_DEPTH 0x10
#define CORE0_INTERRUPT_SRC 0x40000060
#define CORE0_TIMER_IRQ_CTRL 0x40000040

void add_timer(void (*callback)(void*), void *arg, uint32_t duration);
int add_task(void (*callback)(void*), void *arg, int32_t prio);
void do_task();
void irq_handler();

#endif /* _KERNEL_IRQ_H_ */