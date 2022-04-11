#ifndef _INTERRUPT_IRQ_H_
#define _INTERRUPT_IRQ_H_
#include <types.h>
#include <list.h>

#define INT_MAX_DEPTH 0x10
#define CORE0_INTERRUPT_SRC 0x40000060
#define CORE0_TIMER_IRQ_CTRL 0x40000040

#define TASK_MODE_LILO 0b0
#define TASK_MODE_FILO 0b1
#define TASK_MODE TASK_MODE_LILO
typedef struct _TaskEntry
{
    int32_t prio;
    void (*callback)(void*);
    void *arg;
    int running;
    struct list_head list;
} TaskEntry;

void add_timer(void (*callback)(void*), void *arg, uint32_t duration);
TaskEntry* add_task(void (*callback)(void*), void *arg, int32_t prio);
void irq_handler();

#endif /* _KERNEL_IRQ_H_ */