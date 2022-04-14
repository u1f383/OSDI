#ifndef _ARM_MM_H_
#define _ARM_MM_H_
#include <types.h>
#include <list.h>

static addr_t phys_mem_start = 0, \
              phys_mem_end = 0x3B400000;
#define MM_PHYS_MEMORY_START
#define PAGE_SIZE (4 * KB)
#define PAGE_SHIFT 12
#define PFN_BASE_OFFSET (phys_mem_start >> PAGE_SHIFT)

#define phys_to_pfn(_phys_addr) ((((addr_t) _phys_addr) >> PAGE_SHIFT) - PFN_BASE_OFFSET)
#define pfn_to_phys(pfn) ( (pfn + PFN_BASE_OFFSET) << PAGE_SHIFT )

#define page_to_pfn(_page) ( (((uint64_t) _page) - ((uint64_t) mem_map)) / sizeof(Page) )
#define pfn_to_page(pfn) (mem_map + pfn)

#define page_to_phys(_page)      pfn_to_phys( page_to_pfn(_page) )
#define phys_to_page(_phys_addr) pfn_to_page( phys_to_pfn(_phys_addr) )

#define align_page(phys) ( (phys + ((1 << PAGE_SHIFT) - 1)) & ~((1 << PAGE_SHIFT) - 1) )

typedef struct _Page {
    /* Used to check the page status */
    uint8_t flags;
    #define PAGE_FLAG_FREED (0)
    #define PAGE_FLAG_RSVD  (1<<0)
    #define PAGE_FLAG_ALLOC (1<<1)
    #define PAGE_FLAG_BODY  (1<<2)
    
    /* Order of buddy system, and used to check the page is head or body, too */
    int8_t order;
    #define PAGE_ORDER_MAX  11
    #define PAGE_ORDER_BODY (-1)
    #define PAGE_ORDER_UND  (-2)
} Page;

typedef struct _FreeArea {
    struct list_head list;
} FreeArea;

static const uint32_t slab_size_pool[] = \
{
    16, 32, 48, 96, 128, 256, 512, 1024,
    2048, 3172, 4096, 8192, 16384, 32768, 65536
};
#define SLAB_POOL_SIZE (sizeof(slab_size_pool) / sizeof(slab_size_pool[0]))
typedef struct _Slab {
    struct list_head list;
} Slab;
typedef struct _SlabCache {
    uint32_t size;
    addr_t start;
    addr_t end;
    Slab slab;
    struct list_head cache_list;
} SlabCache;
extern SlabCache *slab_cache_ptr[SLAB_POOL_SIZE];

void page_init();
void buddy_init();
void slab_init();
void memory_reserve(uint64_t start, uint64_t end);

char* buddy_alloc(uint32_t req_pgcnt);
char* kmalloc(uint32_t sz);
int32_t kfree(char *chk);
int32_t buddy_free(char *chk);

#endif /* _ARM_MM_H_ */