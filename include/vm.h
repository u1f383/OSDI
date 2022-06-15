#ifndef _VM_H_
#define _VM_H_

#include <types.h>
#include <list.h>

#define MAIR_IDX_NORMAL_NOCACHE 1
#define PD_TABLE     0b11
#define PD_BLOCK     0b01
#define PD_TABLE_ENT 0b11

#define LOWER_ATTR_MASK ((1 << 11) - 1)
#define UPPER_ATTR_MASK (0b111111111111L << 52)
#define ATTR_MASK (LOWER_ATTR_MASK + UPPER_ATTR_MASK)

#define AF_ACCESS (1 << 10)
#define PTE_AP_RDWR     (0b01 << 6)
#define PTE_AP_RDONLY   (0b11 << 6)
#define PTE_AP_NOACCESS (0b00 << 6)
#define PTE_UXN (1L << 54)
#define PTE_PXN (1L << 53)

#define BASE_PTE_ATTR (AF_ACCESS | (MAIR_IDX_NORMAL_NOCACHE << 2) | PD_TABLE_ENT)

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
    uint64_t attr;
    uint64_t prot;
    int flags;
    const char *data;
    struct list_head list;
} vm_area_struct;

typedef struct _mm_struct {
    vm_area_struct *mmap;
    pgd_t *pgd;
} mm_struct;

#define PROT_NONE  0x0
#define PROT_EXEC  0x4
#define PROT_WRITE 0x2
#define PROT_READ  0x1
#define MAP_ANONYMOUS 0x20
#define MAP_POPULATE 0x008000

void dup_pages(void *parent, void *child, int level);
void dup_vma(mm_struct *parent_mm, mm_struct *child_mm);
void do_page_fault(uint64_t far, uint32_t esr);
void *svc_mmap(void* addr, uint64_t len, int prot, int flags, int fd, int file_offset);
void release_vma(mm_struct *mm);
void release_pgtable(void *pagetable, int level);
void mappages(void *pgd, uint64_t va, uint64_t size, uint64_t pa, uint64_t attr);
pte_t *walk(void *pgd, uint64_t va);
vm_area_struct *mmap_internal(mm_struct *mm, void* addr, uint64_t len, int prot, int flags);
vm_area_struct *find_vma(mm_struct *mm, uint64_t addr);

#endif /* _VM_H_ */