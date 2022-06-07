#ifndef _VM_H_
#define _VM_H_

#include <types.h>

#define MAIR_IDX_NORMAL_NOCACHE 1
#define PD_TABLE     0b11
#define PD_BLOCK     0b01
#define PD_TABLE_ENT 0b11
#define AF_ACCESS (1 << 10)
#define AP_ACCESS (1 << 6)

#define PTE_ATTR (AF_ACCESS | AP_ACCESS | (MAIR_IDX_NORMAL_NOCACHE << 2) | PD_TABLE_ENT)

#define PGD_BIT 39
#define PUD_BIT 30
#define PMD_BIT 21
#define PTE_BIT 12
#define GRANULE_SIZE 9

typedef unsigned long pgd_t;
typedef unsigned long pud_t;
typedef unsigned long pmd_t;
typedef unsigned long pte_t;

typedef struct _vm_area_struct {
    uint64_t vm_start;
	uint64_t vm_end;
    struct _vm_area_struct *vm_next, *vm_prev;
} vm_area_struct;

typedef struct _mm_struct {
    vm_area_struct *mmap;
    pgd_t *pgd;
} mm_struct;

void mappages(void *pgd, uint64_t va, uint64_t size, uint64_t pa);

#endif /* _VM_H_ */