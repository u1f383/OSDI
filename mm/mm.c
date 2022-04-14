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
    #define spin_table_start 0x0
    #define spin_table_end   0x1000
    { spin_table_start, spin_table_end },

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
SlabCache *slab_cache_ptr[SLAB_POOL_SIZE];

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
    gb_pgcnt = (phys_mem_end - phys_mem_start) / PAGE_SIZE;
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
    end = _ceil(end, PAGE_SHIFT);

    Page *pg_iter = phys_to_page(start), \
         *pg_end = phys_to_page(end);

    while (pg_iter != pg_end) {
        pg_iter->flags = PAGE_FLAG_RSVD;
        pg_iter->order = PAGE_ORDER_UND;
        pg_iter++;
    }
    #ifdef DEBUG_MM
        printf("[DEBUG] Reserve memory from 0x%x to 0x%x\r\n", start, end);
    #endif /* DEBUG_MM */
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
    LIST_INIT(fa->list);
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
            pg_iter->flags = PAGE_FLAG_BODY;
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
    #ifdef DEBUG_MM
        printf("[DEBUG] Allocate from buddy\r\n");
    #endif /* DEBUG_MM */
    req_pgcnt = ceiling_2(req_pgcnt);
    int32_t order = log_2(req_pgcnt);

    if (order > PAGE_ORDER_MAX)
        return NULL;

    int32_t curr_order = order;
    while ( curr_order <= PAGE_ORDER_MAX && \
            (uint64_t) free_area[curr_order] == FREEAREA_UND)
        curr_order++;
    
    /* Out of memory */
    if (curr_order > PAGE_ORDER_MAX) {
        #ifdef DEBUG_MM
            printf("[DEBUG] Out of memory with request page count 0x%x\r\n", req_pgcnt);
        #endif /* DEBUG_MM */
        return NULL;
    }

    char *ret = free_area[curr_order];
    del_fa(free_area[curr_order], curr_order);

    Page *pg = phys_to_page(ret);
    
    /* Need to split buddy */
    while (order < curr_order)
    {
        #ifdef DEBUG_MM
            printf("[DEBUG] No buddy 0x%x, split from order 0x%x\r\n", curr_order-1, curr_order);
        #endif /* DEBUG_MM */
        curr_order--;

        Page *next_pg = pg + (1 << curr_order);
        next_pg->order = curr_order;

        FreeArea *fa = page_to_phys(next_pg);
        LIST_INIT(fa->list);
        add_fa(fa, next_pg->order);
    }

    pg->order = order;
    pg->flags = PAGE_FLAG_ALLOC;
    #ifdef DEBUG_MM
        printf("[DEBUG] Req order 0x%x, ret order 0x%x\r\n", order, curr_order);
    #endif /* DEBUG_MM */
    return ret;
}

int32_t buddy_free(char *chk)
{
    #ifdef DEBUG_MM
        printf("[DEBUG] Free to buddy\r\n");
    #endif /* DEBUG_MM */
    Page *pg = phys_to_page(chk);
    if ( (pg->order == PAGE_ORDER_UND || pg->flags == PAGE_FLAG_RSVD)  || /* reserved memory */
         (pg->order == PAGE_ORDER_BODY || pg->flags == PAGE_FLAG_BODY) || /* body */
          pg->flags == PAGE_FLAG_FREED /* head has been freed */)
        return -1;

    Page *cons_chk;
    FreeArea *cons_fa;
    /* Consolidate right */
    while (pg->order < PAGE_ORDER_MAX)
    {
        cons_chk = pg + (1 << pg->order);
        if (cons_chk->order == pg->order &&
            cons_chk->flags == PAGE_FLAG_FREED)
        {
            #ifdef DEBUG_MM
                printf("[DEBUG] Consolidate with right buddy 0x%x, order 0x%x --> 0x%x\r\n", pg->order, pg->order+1);
            #endif /* DEBUG_MM */
            del_fa(page_to_phys(cons_chk), pg->order);

            cons_chk->order = PAGE_ORDER_BODY;
            cons_chk->flags = PAGE_FLAG_BODY;
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
            #ifdef DEBUG_MM
                printf("[DEBUG] Consolidate with left buddy, order 0x%x --> 0x%x\r\n", pg->order, pg->order+1);
            #endif /* DEBUG_MM */
            del_fa(page_to_phys(cons_chk), curr_order);

            prev_cons_chk->order = PAGE_ORDER_BODY;
            prev_cons_chk->flags = PAGE_FLAG_BODY;
            cons_chk->order++;

            prev_cons_chk = cons_chk;
            curr_order++;
            continue;
        }
        break;
    }
    prev_cons_chk->flags = PAGE_FLAG_FREED;
    cons_fa = page_to_phys(prev_cons_chk);
    add_fa(cons_fa, curr_order);

    return 0;
}

SlabCache* slab_cache_new(uint32_t sz)
{
    SlabCache *new_cache = buddy_alloc(32);
    new_cache->size = sz;
    new_cache->start = ((char *) new_cache) + sizeof(SlabCache);
    new_cache->end = ((char *) new_cache) + (32*PAGE_SIZE);
                        
    LIST_INIT(new_cache->slab.list);
    LIST_INIT(new_cache->cache_list);
    new_cache->end -= ((32*PAGE_SIZE - sizeof(SlabCache)) % new_cache->size);

    Slab *slab_chk = new_cache + 1;
    while (slab_chk < new_cache->end)
    {
        LIST_INIT(slab_chk->list);
        list_add(&slab_chk->list, &new_cache->slab.list);
        slab_chk = (char *) slab_chk + sz;
    }

    return new_cache;
}

void slab_init()
{
    if (!is_buddy_init)
        return;

    for (int i = 0; i < SLAB_POOL_SIZE; i++)
        slab_cache_ptr[i] = slab_cache_new(slab_size_pool[i]);
}

char *slab_alloc(uint32_t sz)
{
    for (int i = 0; i < SLAB_POOL_SIZE; i++)
        if (slab_size_pool[i] >= sz) {
            SlabCache *curr_cache = slab_cache_ptr[i];
            Slab *next_slab;

            while (1)
            {
                next_slab = container_of(curr_cache->slab.list.next, Slab, list);
                if (next_slab != &curr_cache->slab) {
                    list_del(&next_slab->list);
                    goto _slab_alloc_ok;
                }
                
                curr_cache = container_of(curr_cache->cache_list.next, SlabCache, cache_list);
                if (curr_cache == slab_cache_ptr[i])
                    break;
            }

            #ifdef DEBUG_MM
                printf("[DEBUG] Create new slab cache\r\n");
            #endif /* DEBUG_MM */
            SlabCache *new_cache = slab_cache_new(slab_size_pool[i]);
            list_add_tail(&new_cache->cache_list, &slab_cache_ptr[i]->cache_list);

            next_slab = container_of(new_cache->slab.list.next, Slab, list);
            list_del(&next_slab->list);

        _slab_alloc_ok:
            #ifdef DEBUG_MM
                printf("[DEBUG] Allocate from slab_cache with slab size 0x%x\r\n", slab_size_pool[i]);
            #endif /* DEBUG_MM */
            return next_slab;
        }
    
    return NULL;
}

int32_t slab_free(char *chk)
{
    for (int i = 0; i < SLAB_POOL_SIZE; i++) {
        SlabCache *curr_cache = slab_cache_ptr[i];

        while (1)
        {
            if (chk >= curr_cache->start && chk < curr_cache->end) {
                Slab *tmp_slab = chk;
                list_add_tail(&tmp_slab->list, &curr_cache->slab.list);

                #ifdef DEBUG_MM
                    printf("[DEBUG] Free to 0x%x slab\r\n", slab_size_pool[i]);
                #endif /* DEBUG_MM */
                return 0;
            }
            curr_cache = container_of(curr_cache->cache_list.next, SlabCache, cache_list);
            if (curr_cache == slab_cache_ptr[i])
                break;
        }
    }
    return -1;
}

char* kmalloc(uint32_t sz)
{
    char *ret;
    if ((ret = slab_alloc(sz)) != NULL)
        return ret;

    uint32_t pg_cnt = (sz + ((1 << PAGE_SHIFT) - 1)) >> PAGE_SHIFT;
    return buddy_alloc(pg_cnt);
}

int32_t kfree(char *chk)
{
    if (slab_free(chk) == 0)
        return 0;

    return buddy_free(chk);
}