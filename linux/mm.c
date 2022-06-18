#include <mm.h>
#include <util.h>
#include <list.h>
#include <types.h>
#include <printf.h>

#define FREEAREA_UND ((void *)0xffffffff)

typedef struct _RsvdMem {
    uint64_t start;
    uint64_t end;
} RsvdMem;

const RsvdMem user_rsvd_memory[] = {
    { spin_table_start, spin_table_end },
    { kern_start, kern_end },
    { su_rsvd_start, su_rsvd_end },
};

uint64_t phys_mem_start = 0, \
         phys_mem_end = 0x3B400000;

FreeArea *free_area[PAGE_ORDER_MAX+1];
Page *mem_map;
uint64_t gb_pgcnt;
SlabCache *slab_cache_ptr[SLAB_POOL_SIZE];

static bool buddy_lock = 0;
static bool slab_lock = 0;

#define is_page_rsvd(pg) (pg->flags & PAGE_FLAG_RSVD)

/**
 * ============ startup allocator ============
 */
static char *su_head = (char *)su_rsvd_start,
            *su_tail = (char *)su_rsvd_end;
char *startup_alloc(uint32_t sz)
{
    if (su_head + sz >= su_tail)
        return NULL;

    char *ret = su_head;
    su_head += sz;

    return ret;
}

void page_init()
{
    gb_pgcnt = (phys_mem_end - phys_mem_start) / PAGE_SIZE;
    mem_map = (Page *)startup_alloc(sizeof(Page) * gb_pgcnt);

    for (int i = 0; i <= PAGE_ORDER_MAX; i++)
        free_area[i] = (FreeArea *)FREEAREA_UND;

    for (int i = 0; i < sizeof(user_rsvd_memory) / sizeof(user_rsvd_memory[0]); i++)
        register_mem_reserve( user_rsvd_memory[i].start, user_rsvd_memory[i].end );
}

void register_mem_reserve(uint64_t start, uint64_t end)
{
    start = _floor(start, PAGE_SHIFT);
    end = _ceil(end, PAGE_SHIFT);

    Page *pg_iter = virt_to_page(start), \
         *pg_end = virt_to_page(end);

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
    if (free_area[order] == FREEAREA_UND)
        free_area[order] = fa;
    else
        list_add_tail(&fa->list, &free_area[order]->list);
}

void buddy_init()
{
    Page *pg_iter = mem_map, \
         *pg_end  = mem_map + gb_pgcnt;

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

        pg_cnt = ((uint64_t)pg_iter - (uint64_t)start_pg) / sizeof(Page);
        while (pg_cnt)
        {
            order = log_2(pg_cnt);
            order = (order > PAGE_ORDER_MAX) ? PAGE_ORDER_MAX : order;
            start_pg->order = order;
            start_pg->flags = PAGE_FLAG_FREED;

            FreeArea *fa = (void *)page_to_virt(start_pg);
            add_fa(fa, order);

            start_pg += (1 << order);
            pg_cnt -= (1 << order);
        }

        if (pg_iter == pg_end)
            break;
    }
}

int32_t buddy_inc_refcnt(void *chk)
{
    Page *pg = virt_to_page(chk);
    if ((pg->order == PAGE_ORDER_UND || pg->flags == PAGE_FLAG_RSVD)  || /* reserved memory */
        (pg->order == PAGE_ORDER_BODY || pg->flags == PAGE_FLAG_BODY) || /* body */
         pg->flags == PAGE_FLAG_FREED /* head has been freed */)
        return -1;

    pg->refcnt++;
    return 0;
}

void* buddy_alloc(uint32_t req_pgcnt)
{
    #ifdef DEBUG_MM
    printf("[DEBUG] Allocate from buddy\r\n");
    #endif /* DEBUG_MM */

    req_pgcnt = ceiling_2(req_pgcnt);
    int32_t order = log_2(req_pgcnt);

    if (order > PAGE_ORDER_MAX)
        return NULL;

    while (buddy_lock);
    buddy_lock = 1;

    int32_t curr_order = order;
    while (curr_order <= PAGE_ORDER_MAX && \
           free_area[curr_order] == FREEAREA_UND)
        curr_order++;
    
    /* Out of memory */
    if (curr_order > PAGE_ORDER_MAX) {
        #ifdef DEBUG_MM
        printf("[DEBUG] Out of memory with request page count 0x%x\r\n", req_pgcnt);
        #endif /* DEBUG_MM */
        buddy_lock = 0;
        return NULL;
    }

    void *ret = free_area[curr_order];
    del_fa(free_area[curr_order], curr_order);

    Page *pg = virt_to_page(ret);
    
    /* Need to split buddy */
    while (order < curr_order)
    {
        #ifdef DEBUG_MM
        printf("[DEBUG] No buddy 0x%x, split from order 0x%x\r\n", curr_order-1, curr_order);
        #endif /* DEBUG_MM */
        curr_order--;

        Page *next_pg = pg + (1 << curr_order);
        next_pg->order = curr_order;

        FreeArea *fa = (void *)page_to_virt(next_pg);
        LIST_INIT(fa->list);
        add_fa(fa, next_pg->order);
    }

    pg->order = order;
    pg->flags = PAGE_FLAG_ALLOC;
    pg->refcnt = 1;
    #ifdef DEBUG_MM
    printf("[DEBUG] Req order 0x%x, ret order 0x%x\r\n", order, curr_order);
    #endif /* DEBUG_MM */
    
    buddy_lock = 0;
    return ret;
}

int32_t buddy_free(void *chk)
{
    #ifdef DEBUG_MM
    printf("[DEBUG] Free to buddy\r\n");
    #endif /* DEBUG_MM */
    
    while (buddy_lock);
    buddy_lock = 1;

    Page *pg = virt_to_page(chk);
    if ((pg->order == PAGE_ORDER_UND || pg->flags == PAGE_FLAG_RSVD)  || /* reserved memory */
        (pg->order == PAGE_ORDER_BODY || pg->flags == PAGE_FLAG_BODY) || /* body */
         pg->flags == PAGE_FLAG_FREED /* head has been freed */)
    {
        buddy_lock = 0;
        return -1;
    }

    pg->refcnt--;
    if (pg->refcnt) {
        buddy_lock = 0;
        return 0;
    }

    Page *merged_chk;
    /* Consolidate right */
    while (pg->order < PAGE_ORDER_MAX)
    {
        merged_chk = pg + (1 << pg->order);
        if (merged_chk->order != pg->order ||
            merged_chk->flags != PAGE_FLAG_FREED)
            break;
        
        #ifdef DEBUG_MM
        printf("[DEBUG] Consolidate with right buddy 0x%x, order 0x%x --> 0x%x\r\n", pg->order, pg->order+1);
        #endif /* DEBUG_MM */
        del_fa((FreeArea *)page_to_virt(merged_chk), pg->order);

        merged_chk->order = PAGE_ORDER_BODY;
        merged_chk->flags = PAGE_FLAG_BODY;
        pg->order++;
    }

    uint8_t curr_order = pg->order;
    Page *prev_merged_chk = pg;
    /* Consolidate left */
    while (curr_order < PAGE_ORDER_MAX)
    {
        merged_chk = pg - (1 << curr_order);
        if (merged_chk->order != curr_order ||
            merged_chk->flags != PAGE_FLAG_FREED)
            break;

        #ifdef DEBUG_MM
            printf("[DEBUG] Consolidate with left buddy, order 0x%x --> 0x%x\r\n", pg->order, pg->order+1);
        #endif /* DEBUG_MM */
        del_fa((FreeArea *)page_to_virt(merged_chk), curr_order);

        prev_merged_chk->order = PAGE_ORDER_BODY;
        prev_merged_chk->flags = PAGE_FLAG_BODY;
        merged_chk->order++;

        prev_merged_chk = merged_chk;
        curr_order++;
    }

    prev_merged_chk->flags = PAGE_FLAG_FREED;
    add_fa((FreeArea *)page_to_virt(prev_merged_chk), curr_order);

    buddy_lock = 0;
    return 0;
}

SlabCache* slab_cache_new(uint32_t sz)
{
    #define SLAB_PER_PAGECNT 64
    
    SlabCache *new_cache = buddy_alloc(SLAB_PER_PAGECNT);
    new_cache->size = sz;
    new_cache->start = (uint64_t)new_cache + sizeof(SlabCache);
    new_cache->end = (uint64_t)new_cache + (SLAB_PER_PAGECNT * PAGE_SIZE);
                        
    LIST_INIT(new_cache->slab.list);
    LIST_INIT(new_cache->cache_list);
    new_cache->end -= (SLAB_PER_PAGECNT * PAGE_SIZE - sizeof(SlabCache)) % new_cache->size;

    Slab *slab_chk = (Slab *)(new_cache + 1);
    while ((uint64_t)slab_chk < new_cache->end)
    {
        LIST_INIT(slab_chk->list);
        list_add_tail(&slab_chk->list, &new_cache->slab.list);
        slab_chk = (Slab *)((char *)slab_chk + sz);
    }

    return new_cache;
}

void slab_init()
{
    for (int i = 0; i < SLAB_POOL_SIZE; i++)
        slab_cache_ptr[i] = slab_cache_new(slab_size_pool[i]);
}

void *slab_alloc(uint32_t sz)
{
    while (slab_lock);
    slab_lock = 1;

    for (int i = 0; i < SLAB_POOL_SIZE; i++) {
        if (slab_size_pool[i] >= sz) {
            SlabCache *curr_cache = slab_cache_ptr[i];
            Slab *next_slab;

            while (1)
            {
                next_slab = container_of(curr_cache->slab.list.next, Slab, list);
                if (next_slab != &curr_cache->slab) {
                    list_del(&next_slab->list);
                    goto slab_alloc_ok;
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

        slab_alloc_ok:
            #ifdef DEBUG_MM
            printf("[DEBUG] Allocate from slab_cache with slab size 0x%x\r\n", slab_size_pool[i]);
            #endif /* DEBUG_MM */
            
            slab_lock = 0;
            return next_slab;
        }
    }
    
    slab_lock = 0;
    return NULL;
}

int32_t slab_free(void *chk)
{
    while (slab_lock);
    slab_lock = 1;

    for (int i = 0; i < SLAB_POOL_SIZE; i++) {
        SlabCache *curr_cache = slab_cache_ptr[i];

        while (1)
        {
            if ((uint64_t)chk >= curr_cache->start && (uint64_t)chk < curr_cache->end) {
                Slab *tmp_slab = chk;
                list_add_tail(&tmp_slab->list, &curr_cache->slab.list);

                #ifdef DEBUG_MM
                printf("[DEBUG] Free to 0x%x slab\r\n", slab_size_pool[i]);
                #endif /* DEBUG_MM */
                slab_lock = 0;
                return 0;
            }
            
            curr_cache = container_of(curr_cache->cache_list.next, SlabCache, cache_list);
            if (curr_cache == slab_cache_ptr[i])
                break;
        }
    }

    slab_lock = 0;
    return -1;
}

void* kmalloc(uint32_t sz)
{
    void *ret;
    if ((ret = slab_alloc(sz)) != NULL)
        return ret;

    uint32_t pg_cnt = (sz + PAGE_SIZE - 1) >> PAGE_SHIFT;
    return buddy_alloc(pg_cnt);
}

int32_t kfree(void *chk)
{
    if (slab_free(chk) == 0)
        return 0;

    return buddy_free(chk);
}