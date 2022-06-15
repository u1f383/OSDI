#ifndef _MM_H_
#define _MM_H_

#include <types.h>
#include <list.h>

#define PAGE_SIZE (4 * KB)
#define PAGE_SHIFT 12
#define PAGE_OFFSET_MASK (0xfff)
#define MM_VIRT_KERN_START 0xFFFF000000000000
#define PFN_BASE_OFFSET (phys_mem_start >> PAGE_SHIFT)

#define phys_to_pfn(_phys_addr) ((((uint64_t) _phys_addr) >> PAGE_SHIFT) - PFN_BASE_OFFSET)
#define pfn_to_phys(pfn) ( (pfn + PFN_BASE_OFFSET) << PAGE_SHIFT )
#define pfn_to_virt(pfn) ( pfn_to_phys(pfn) + MM_VIRT_KERN_START )

#define page_to_pfn(_page) ( (((uint64_t) _page) - ((uint64_t) mem_map)) / sizeof(Page) )
#define pfn_to_page(pfn) (mem_map + pfn)

#define page_to_phys(_page)      pfn_to_phys( page_to_pfn(_page) )
#define page_to_virt(_page)      pfn_to_virt( page_to_pfn(_page) )
#define phys_to_page(_phys_addr) pfn_to_page( phys_to_pfn(_phys_addr) )
#define virt_to_page(_virt_addr) pfn_to_page( phys_to_pfn(((uint64_t)_virt_addr) - MM_VIRT_KERN_START) )

#define virt_to_phys(_virt_addr) (((uint64_t)_virt_addr) - MM_VIRT_KERN_START)
#define phys_to_virt(_phys_addr) (((uint64_t)_phys_addr) + MM_VIRT_KERN_START)

#define align_page(phys) ( (phys + ((1 << PAGE_SHIFT) - 1)) & ~((1 << PAGE_SHIFT) - 1) )

#define spin_table_start 0xFFFF000000000000
#define spin_table_end   0xFFFF000000003000
#define kern_start       0xFFFF000000080000
#define kern_end         0xFFFF000000100000
#define su_rsvd_start    0xFFFF000000100000
#define su_rsvd_end      0xFFFF000000200000

#define PAGE_ROUNDUP(addr) (((addr) + 0xfff) & ~0xfff)

extern uint64_t phys_mem_end;

typedef struct _Page {
    /* Used to check the page status */
    uint16_t flags : 4;
    #define PAGE_FLAG_FREED 0
    #define PAGE_FLAG_RSVD  (1<<0)
    #define PAGE_FLAG_ALLOC (1<<1)
    #define PAGE_FLAG_BODY  (1<<2)
    
    /* Order of buddy system, and used to check the page is head or body, too */
    uint16_t order : 4;
    #define PAGE_ORDER_MAX  11
    #define PAGE_ORDER_BODY 0b1111
    #define PAGE_ORDER_UND  0b1110

    uint16_t refcnt : 8;
} Page;

typedef struct _FreeArea {
    struct list_head list;
} FreeArea;

static const uint32_t slab_size_pool[] = \
{
    0x10, 0x20, 0x30, 0x60, 0x80, 0x100, 0x200, 0x400,
    0x800, 0x1000, 0x2000, 0x3000, 0x4000, 0x6000, 0x8000
};

#define SLAB_POOL_SIZE (sizeof(slab_size_pool) / sizeof(slab_size_pool[0]))

typedef struct _Slab {
    struct list_head list;
} Slab;

typedef struct _SlabCache {
    uint32_t size;
    uint64_t start;
    uint64_t end;
    Slab slab;
    struct list_head cache_list;
} SlabCache;
extern SlabCache *slab_cache_ptr[SLAB_POOL_SIZE];

void page_init();
void buddy_init();
void slab_init();
void register_mem_reserve(uint64_t start, uint64_t end);

int32_t buddy_inc_refcnt(void *chk);

void* buddy_alloc(uint32_t req_pgcnt);
void* kmalloc(uint32_t sz);
int32_t kfree(void *chk);
int32_t buddy_free(void *chk);

#endif /* _MM_H_ */