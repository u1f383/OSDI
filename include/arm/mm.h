#ifndef _ARM_MM_H_
#define _ARM_MM_H_
#include <types.h>
#include <list.h>

#define MM_PHYS_MEMORY_START 0
#define MM_PHYS_MEMORY_END (1 * GB)
#define PAGE_SIZE (4 * KB)
#define PAGE_SHIFT 12
#define PFN_BASE_OFFSET (MM_PHYS_MEMORY_START >> PAGE_SHIFT)

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
    
    /* Order of buddy system, and used to check the page is hdr or body, too */
    int8_t order;
    #define PAGE_ORDER_MAX  11
    #define PAGE_ORDER_BODY (-1)
} Page;

typedef struct _FreeArea {
    struct list_head list;
} FreeArea;

void page_init();
void buddy_init();
void memory_reserve(uint64_t start, uint64_t end);

char* buddy_alloc(uint32_t req_pgcnt);
void buddy_free(char *chk);

#endif /* _ARM_MM_H_ */