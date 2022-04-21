#ifndef _KERNEL_KERNEL_H_
#define _KERNEL_KERNEL_H_

#include <types.h>

void from_el1_to_el0(void(*prog)(), uint64_t stack);
void ret_from_fork();

#endif /* _KERNEL_KERNEL_H_ */