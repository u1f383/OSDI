#ifndef _INTERRUPT_SVC_H_
#define _INTERRUPT_SVC_H_
#include <types.h>

#define SVC_NUM 0x10
extern uint64_t svc_table[SVC_NUM];

#endif /* _INTERRUPT_SVC_H_ */