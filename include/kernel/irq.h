#ifndef _KERNEL_IRQ_H_
#define _KERNEL_IRQ_H_
#include <types.h>

#define INT_MAX_DEPTH 0x10
#define CORE0_INTERRUPT_SRC 0x40000060

typedef struct _IntState
{
    reg32 spsr_el1;
    reg32 elr_el1;
} IntState;

extern IntState int_stack[ INT_MAX_DEPTH ];

#endif /* _KERNEL_IRQ_H_ */