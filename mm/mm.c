#include <arm/mm.h>
#include <util.h>
#include <list.h>

static uint64_t spfn, epfn;
Page *mem_map;
Pg_data *_pg_data;
addr_t pg_data_addr;

#define phys_to_pfn(_phys_addr) ((((addr_t) _phys_addr) >> PAGE_SHIFT) - PFN_BASE_OFFSET)
#define pfn_to_phys(pfn) ( (pfn + PFN_BASE_OFFSET) << PAGE_SHIFT )

#define page_to_pfn(_page) ( (((uint64_t) _page) - ((uint64_t) mem_map)) / sizeof(Page) )
#define pfn_to_page(pfn) (mem_map + pfn)

#define page_to_phys(_page)      pfn_to_phys( page_to_pfn(_page) )
#define phys_to_page(_phys_addr) pfn_to_page( phys_to_pfn(_phys_addr) )

/**
virt_to_page() returns the page associated with a virtual address
pfn_to_page() returns the page associated with a page frame number
page_address() returns the virtual address of a Page; this functions can be called only for pages from lowmem
kmap() creates a mapping in kernel for an arbitrary physical page (can be from highmem) and returns a virtual address that can be used to directly reference the page
*/

#define INIT_PAGE(_page)                                 \
    do {                                                 \
        _page->list = (struct list_head) { NULL, NULL }; \
        _page->flags = PG_init;                          \
        _page->order = NON_ORDER;                        \
        _page->private.free_pg = NULL;                   \
        _page->index = UN_INDEX;                         \
    } while (0)

void mem_map_init()
{
    addr_t mem_start = MM_PHYS_MEMORY_START;
    addr_t mem_end = MM_PHYS_MEMORY_END;

    spfn = 0;
    epfn = phys_to_pfn(mem_end);

    /* Initialize global mem_map */
    mem_map = (Page *) mem_start;
    INIT_PAGE(mem_map);
    LIST_INIT(mem_map->list);

    Page *curr;
    for (int i = spfn + 1; i < epfn; i++)
    {
        curr = pfn_to_page(i);
        INIT_PAGE(curr);
        list_add_tail(&curr->list, &mem_map->list);
    }

    /* _pg_data struct is followed by mem_map */
    pg_data_addr = (addr_t) pfn_to_page(epfn);
}

void buddy_init(struct free_area *free_area,
                int32_t order,
                uint64_t _spfn,
                uint64_t _epfn)
{
    Page *pg_cur = NULL, *pg_prev = NULL;
    uint64_t idx = 0;
    uint32_t cnt = 0;

    for (int i = _spfn; i < _epfn;)
    {
        idx = i;
        pg_cur = pfn_to_page(i);
        pg_cur->flags = PG_buddy;
        pg_cur->order = order;
        pg_cur->index = idx;
        
        if (pg_prev)
            pg_prev->private.free_pg = pg_cur;
        pg_prev = pg_cur;
        
        if (i + (1 << order) >= _epfn)
            break;

        cnt++; i++;
        pg_cur = container_of(pg_cur->list.next, Page, list);
        while ( i % (1 << order) )
        {
            pg_cur->order = BODY_ORDER;
            pg_cur->flags = PG_buddy;
            pg_cur->index = idx;
            pg_cur = container_of(pg_cur->list.next, Page, list);
            i++;
        }
    }

    pg_cur = pfn_to_page(_spfn);
    free_area->free_list = &pg_cur->list;
    free_area->nr_free = cnt;
}

void *buddy_alloc(uint64_t pg_sz)
{
    uint64_t ceil_sz = ceiling_2(pg_sz);
    int32_t order = log_2(ceil_sz);
    Zone *node_zone = _pg_data->node_zones;

    if (order >= MAX_ORDER)
        return NULL;

    Page *buddy = NULL;
    while (order < MAX_ORDER)
    {
        if (!node_zone->free_area[order].free_list) {
            order++;
            continue;
        }
        
        buddy = container_of(node_zone->free_area[order].free_list, Page, list);
        if (buddy->private.free_pg)
            node_zone->free_area[order].free_list = &buddy->private.free_pg->list;
        else
            node_zone->free_area[order].free_list = NULL;
        
        /* Split into 2 buddies */
        while ((1 << order) >= ceil_sz)
        {
            buddy_split_2(buddy, node_zone);
            order--;
        }
        break;
    }

    if (!buddy)
        return NULL;

    Page *pg_cur = buddy;
    for (int i = 0; i < (1 << order); i++) {
        pg_cur->order = ALLOC_ORDER;
        pg_cur->flags = PG_inuse;
        pg_cur->private.free_pg = NULL;
        pg_cur = container_of(pg_cur->list.next, Page, list);
    }

    return (void *) page_to_phys(buddy);
}

/* HAS BUG */
void buddy_split_2(Page *buddy_hdr, Zone *zone)
{
    int32_t new_order = buddy_hdr->order - 1;
    Page *buddy_2 = buddy_hdr + (1 << new_order);

    /* Update buddy */
    buddy_hdr->order = new_order;

    /* Update buddy2 */
    buddy_2->order = new_order;
    buddy_2->flags = PG_buddy;
    buddy_2->index = page_to_pfn(buddy_2);

    Page *pg_cur = container_of(buddy_2->list.next, Page, list);
    for (int i = 1; i < (1 << new_order); i++)
    {
        pg_cur->index = buddy_2->index;
        pg_cur = container_of(pg_cur->list.next, Page, list);
    }

    /* Update buddy2 freelist */
    if (zone->free_area[new_order].free_list)
        buddy_2->private.free_pg = container_of(zone->free_area[new_order].free_list, Page, list);
    else
        buddy_2->private.free_pg = NULL;
    zone->free_area[new_order].free_list = &buddy_2->list;
}

void buddy_free(void *phys_addr)
{
    // uint64_t pg_sz = 0;
    // Page *buddy = phys_to_page(phys_addr);

    // Page *pg_cur = buddy;
    // while (pg_cur->index == buddy->index)
    // {
    //     pg_cur->flags = PG_buddy;
    //     pg_cur->order = BODY_ORDER;
    //     pg_cur->private.free_pg = NULL;
    //     pg_cur = container_of(pg_cur->list.next, Page, list);
        
    //     pg_sz++;
    // }

    // int32_t order = log_2(pg_sz);
    // buddy->order = order;

    // /* Insert into corresponding buddy system order */
    // Zone *node_zone = _pg_data->node_zones;
    // buddy->free_list = node_zone->free_area[order].free_list;
    // node_zone->free_area[order].free_list = &buddy->list;
}

static void _zone_dma_init(Pg_data *pgdat, Zone *zone)
{
    zone->zone_mem_map = mem_map;
    zone->zone_pgdat = pgdat;

    for (int i = 0; i < MAX_ORDER; i++)
        zone->free_area[i].free_list = NULL;

    buddy_init(&zone->free_area[MAX_ORDER - 1],
                MAX_ORDER - 1, spfn, epfn);
}

void node_init()
{
    _pg_data = (Pg_data *) pg_data_addr;
    _pg_data->node_mem_map = mem_map;

    _zone_dma_init(_pg_data, &_pg_data->node_zones[ ZONE_DMA ]);
}