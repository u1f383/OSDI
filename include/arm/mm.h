#ifndef _ARM_MM_H_
#define _ARM_MM_H_

/**
 * The tech of this memory management mechanism is refered from
 * https://www.kernel.org/doc/gorman/html/understand/understand005.html
 */

#include <util.h>
#include <types.h>
#include <list.h>

#define MM_PHYS_MEMORY_START 0x21000000
#define MM_PHYS_MEMORY_END (1 * GB)
#define MM_PHYS_MEMORY_RESERVED (32 * MB)
#define NR_PAGE 0x1f000 /* (END - START) / PAGE_SIZE */
#define MM_MEMORY_BIT 30 /* 1GB */
#define PAGE_SIZE (4 * KB)
#define PAGE_SHIFT 12

#define PFN_BASE_OFFSET ((MM_PHYS_MEMORY_START + MM_PHYS_MEMORY_RESERVED) >> PAGE_SHIFT)

/**
 * Buddy System Implementation
 * @ Maintain continuous block of physical memory
 * @ https://blog.csdn.net/weixin_48101150/article/details/108940623
 */

/* (2^i) * PAGE_SIZE */
#define NON_ORDER (-64)
#define BODY_ORDER (-128)
#define ALLOC_ORDER (-256)
#define MAX_ORDER 11
#define PAGE_BUDDY_MAPCOUNT_VALUE (-128)

struct free_area {
    /* Point to first free page */
    struct list_head *free_list;
    /* The number of free areas of a given size */
    uint32_t nr_free;
};

#define ZONE_DMA_MEM_START MM_PHYS_MEMORY_START
#define ZONE_DMA_SIZE (16 * MB)
#define ZONE_DMA_MEM_END (ZONE_DMA_MEM_START + ZONE_DMA_SIZE - 1)

#define ZONE_NORMAL_MEM_START (ZONE_DMA_MEM_END + 1)
#define ZONE_NORMAL_SIZE (256 * MB)
#define ZONE_NORMAL_MEM_END (ZONE_NORMAL_MEM_START + ZONE_NORMAL_SIZE - 1)

#define ZONE_HIGHMEM_MEM_START (ZONE_NORMAL_MEM_START + 1)
#define ZONE_HIGHMEM_MEM_END MM_PHYS_MEMORY_END
#define ZONE_HIGHMEM_SIZE (ZONE_HIGHMEM_MEM_END - ZONE_HIGHMEM_MEM_START + 1)

#define PG_init  0b00000000
#define PG_buddy 0b00000001
#define PG_inuse 0b00000010

#define UN_INDEX (-1)

typedef struct _Page {
    struct list_head list;
    uint64_t flags;
    int64_t index; /* The pfn of buddy header */
    int32_t order; /* Order of buddy system */
    union
    {
        struct _Page *free_pg;
    } private;
} Page;

/**
 * Zone type
 * @ The zone with different usage has different zone type
 */
enum {
    ZONE_DMA, /* Directly accessing the memory */
    MAX_NR_ZONES
};

struct _Pg_data;
typedef struct _Zone {
    /* Used by buddy system */
    struct free_area free_area[MAX_ORDER];
    /* The first page in the global mem_map this zone refers to */
    Page *zone_mem_map;
    /* Point to parent node */
    struct _Pg_data *zone_pgdat;
} Zone;

/* We only support UMA */
typedef struct _Pg_data {
    /* The zones for this node */
    Zone node_zones[MAX_NR_ZONES];
    /* The first page in the global mem_map this node refers to */
    Page *node_mem_map;
} Pg_data;

void mem_map_init();
void node_init();

void buddy_init(struct free_area *free_area, int32_t order,
                uint64_t _spfn, uint64_t _epfn);

void *buddy_alloc(uint64_t pg_sz);
void buddy_split_2(Page *buddy_hdr, Zone *zone);
void buddy_free(void *phys_addr);

#endif /* _ARM_MM_H_ */