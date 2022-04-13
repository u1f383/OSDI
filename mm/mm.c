#include <arm/mm.h>
#include <util.h>
#include <list.h>
#include <types.h>

#define FREEAREA_UND 0xffffffff

typedef struct _RsvdMem {
    addr_t start;
    addr_t end;
} RsvdMem;

RsvdMem user_rsvd_memory[] = {
    #define kern_start 0x80000
    #define kern_end   0x100000
    { kern_start, kern_end },

    #define user_stack      0x100000
    #define user_stack_size 0x4000
    { user_stack, user_stack + user_stack_size },

    #define su_rsvd_base 0x104000
    #define su_rsvd_size 0xFC000
    { su_rsvd_base, su_rsvd_base + su_rsvd_size },
};

static bool is_buddy_init = 0;
FreeArea *free_area[PAGE_ORDER_MAX+1];
Page *mem_map;
uint64_t gb_pgcnt;

/**
 * ============ startup allocator ============
 */
char *su_head = (char *) su_rsvd_base;
char *su_tail = (char *) (su_rsvd_base + su_rsvd_size);
char *startup_alloc(uint32_t sz)
{
    if (is_buddy_init || su_head + sz >= su_tail)
        return NULL;

    char *ret = su_head;
    su_head += sz;

    return ret;
}

static inline bool is_page_rsvd(Page *pg)
{
    return pg->flags & PAGE_FLAG_RSVD;
}

void page_init()
{
    gb_pgcnt = (MM_PHYS_MEMORY_END - MM_PHYS_MEMORY_START) / PAGE_SIZE;
    mem_map = (Page *) startup_alloc(sizeof(Page) * gb_pgcnt);

    for (int i = 0; i <= PAGE_ORDER_MAX; i++)
        free_area[i] = (FreeArea*) FREEAREA_UND;

    for (int i = 0; i < sizeof(user_rsvd_memory) / sizeof(user_rsvd_memory[0]); i++)
        memory_reserve( user_rsvd_memory[i].start, user_rsvd_memory[i].end );
}

void memory_reserve(uint64_t start, uint64_t end)
{
    if (is_buddy_init)
        return;
    
    start = _floor(start, PAGE_SHIFT);
    end = _floor(end, PAGE_SHIFT);

    Page *pg_iter = phys_to_page(start), \
         *pg_end = phys_to_page(end);

    while (pg_iter != pg_end) {
        pg_iter->flags = PAGE_FLAG_RSVD;
        pg_iter->order = PAGE_ORDER_BODY;
        pg_iter++;
    }
    /* Last one */
    pg_iter->flags = PAGE_FLAG_RSVD;
    pg_iter->order = PAGE_ORDER_BODY;
}

static inline void del_fa(FreeArea *victim, int8_t order)
{
    FreeArea *next = container_of(victim->list.next, FreeArea, list);
    if (next == victim) {
        free_area[order] = FREEAREA_UND;
    } else if (free_area[order] == victim) {
        free_area[order] = next;
        list_del(&victim->list);
    } else {
        list_del(&victim->list);
    }
}

static inline void add_fa(FreeArea *fa, int8_t order)
{
    if ((uint64_t) free_area[order] == FREEAREA_UND)
        free_area[order] = fa;
    else
        list_add_tail(&fa->list, &free_area[order]->list);
}

void buddy_init()
{
    Page *pg_iter = mem_map, \
         *pg_end = mem_map + gb_pgcnt;

    while (1)
    {
        while (is_page_rsvd(pg_iter) && pg_iter != pg_end)
            pg_iter++;

        if (pg_iter == pg_end)
            break;
        
        Page *start_pg = pg_iter;
        int32_t pg_cnt;
        int8_t order;

        while (!is_page_rsvd(pg_iter) && pg_iter != pg_end) {
            pg_iter->order = PAGE_ORDER_BODY;
            pg_iter->flags = PAGE_FLAG_FREED;
            pg_iter++;
        }

        pg_cnt = (((uint64_t) pg_iter) - ((uint64_t) start_pg)) / sizeof(Page);
        while (pg_cnt)
        {
            order = log_2(pg_cnt);
            order = (order > PAGE_ORDER_MAX) ? PAGE_ORDER_MAX : order;
            start_pg->order = order;
            start_pg->flags = PAGE_FLAG_FREED;

            FreeArea *fa = page_to_phys(start_pg);
            LIST_INIT(fa->list);
            add_fa(fa, order);

            start_pg += (1 << order);
            pg_cnt -= (1 << order);
        }

        if (pg_iter == pg_end)
            break;
    }

    is_buddy_init = 1;
}

char* buddy_alloc(uint32_t req_pgcnt)
{
    req_pgcnt = ceiling_2(req_pgcnt);
    int32_t order = log_2(req_pgcnt);

    if (order == -1 || order > PAGE_ORDER_MAX)
        return NULL;

    int32_t curr_order = order;
    while ( curr_order <= PAGE_ORDER_MAX && \
            (uint64_t) free_area[curr_order] == FREEAREA_UND)
        curr_order++;
    
    if (curr_order > PAGE_ORDER_MAX)
        return NULL;

    char *ret = free_area[curr_order];
    del_fa(free_area[curr_order], curr_order);

    Page *pg = phys_to_page(ret);
    Page *pg_iter = pg;
    for (int i = 0; i < req_pgcnt; pg_iter++, i++)
        pg_iter->flags = PAGE_FLAG_ALLOC;
    
    /* Need to split buddy */
    if (order < curr_order)
    {
        pg->order = curr_order-1;
        pg_iter->order = curr_order-1;

        FreeArea *fa = page_to_phys(pg_iter);
        LIST_INIT(fa->list);
        add_fa(fa, pg_iter->order);
    }
    else
        pg->order = order;

    return ret;
}

void buddy_free(char *chk)
{
    Page *pg = phys_to_page(chk);
    if (pg->order == PAGE_ORDER_BODY || pg->flags == PAGE_FLAG_FREED)
        return;

    Page *cons_chk;
    FreeArea *cons_fa;
    /* Consolidate right */
    while (pg->order < PAGE_ORDER_MAX)
    {
        cons_chk = pg + (1 << pg->order);
        if (cons_chk->order == pg->order &&
            cons_chk->flags == PAGE_FLAG_FREED)
        {
            del_fa(page_to_phys(cons_chk), pg->order);

            cons_chk->order = PAGE_ORDER_BODY;
            pg->order++;
            continue;
        }
        break;
    }

    uint8_t curr_order = pg->order;
    Page *prev_cons_chk = pg;
    /* Consolidate left */
    while (curr_order < PAGE_ORDER_MAX)
    {
        cons_chk = pg - (1 << curr_order);
        if (cons_chk->order == curr_order &&
            cons_chk->flags == PAGE_FLAG_FREED)
        {
            del_fa(page_to_phys(cons_chk), curr_order);

            prev_cons_chk->order = PAGE_ORDER_BODY;
            cons_chk->order++;

            prev_cons_chk = cons_chk;
            curr_order++;
            continue;
        }
        break;
    }

    cons_fa = page_to_phys(prev_cons_chk);
    add_fa(cons_fa, curr_order);
}