#include <types.h>

#define SVC_NUM 0x10

uint64_t svc_table[SVC_NUM];

void svc0_handler();