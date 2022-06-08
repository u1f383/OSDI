#include <sched.h>
#include <irq.h>
#include <gpio.h>
#include <types.h>
#include <util.h>
#include <printf.h>
#include <mm.h>

static uint64_t jiffies = 0;

typedef struct _TimeJob
{
    uint32_t duration;
    void (*callback)(void*);
    void *arg;
    struct list_head list;
} TimeJob;

typedef struct _BottomHalfJob
{
    int32_t prio;
    void (*callback)(void*);
    void *arg;
    int running;
    struct list_head list;
} BottomHalfJob;
#define BHJ_POOL_SIZE 0x10
static BottomHalfJob bhj_pool[BHJ_POOL_SIZE];
static uint16_t bhj_pool_bitmap = 0xffff;

static struct list_head time_jobs_hdr = LIST_HEAD_INIT(time_jobs_hdr);
static struct list_head bhj_hdr = LIST_HEAD_INIT(bhj_hdr);

#define IS_TIME_JOB_EMPTY (time_jobs_hdr.next == &time_jobs_hdr)
#define IS_BHJ_EMPTY (bhj_hdr.next == &bhj_hdr)

void add_timer(void (*callback)(void*), void *arg, uint32_t duration);
BottomHalfJob *add_bhj(void (*callback)(void*), void *arg, int32_t prio);

TimeJob *new_time_job(void (*callback)(void*), void *arg, uint32_t duration)
{
    TimeJob *time_job = kmalloc(sizeof(TimeJob));
    time_job->callback = callback;
    time_job->arg = arg;
    time_job->duration = duration;
    LIST_INIT(time_job->list);
    return time_job;
}

static BottomHalfJob *__alloc_bhj()
{
    for (int i = 0; i < BHJ_POOL_SIZE; i++) {
        if (bhj_pool_bitmap & (1 << i)) {
            bhj_pool_bitmap ^= (1 << i);
            return &bhj_pool[i];
        }
    }
    return NULL;
}

static void __free_bhj(BottomHalfJob *bhj)
{
    for (int i = 0; i < BHJ_POOL_SIZE; i++) {
        if (bhj == &bhj_pool[i]) {
            bhj_pool_bitmap |= (1 << i);
            return;
        }
    }
}

BottomHalfJob *new_bhj(void (*callback)(void*), void *arg, int32_t prio)
{
    BottomHalfJob *bhj = __alloc_bhj();
    bhj->prio = prio;
    bhj->callback = callback;
    bhj->arg = arg;
    bhj->running = 0;
    LIST_INIT(bhj->list);
    return bhj;
}

void add_timer(void(*callback)(void*), void *arg, uint32_t second)
{
    disable_intr();
    
    if (second == 0) { /* Invalid */
        enable_intr();
        return;
    }
    
    /* Second to time slot */
    uint32_t frq = read_sysreg(cntfrq_el0);
    uint32_t duration = second * frq;
    duration /= TIME_UNIT;

    TimeJob *time_job = new_time_job(callback, arg, duration);

    if (IS_TIME_JOB_EMPTY) {
        list_add(&time_job->list, &time_jobs_hdr);
    } else {
        TimeJob *first_tj = container_of(time_jobs_hdr.next, TimeJob, list);
        if (duration <= first_tj->duration)
            list_add(&time_job->list, &time_jobs_hdr);
        else
            list_add_tail(&time_job->list, &time_jobs_hdr);
    }

    enable_intr();
}

BottomHalfJob *add_bhj(void (*callback)(void*), void *arg, int32_t prio)
{
    BottomHalfJob *bhj = new_bhj(callback, arg, prio);

    if (IS_BHJ_EMPTY) {
        list_add(&bhj->list, &bhj_hdr);
    } else {
        BottomHalfJob *first = container_of(bhj_hdr.next, BottomHalfJob, list);
        BottomHalfJob *iter = first;
        
        if (TASK_MODE == TASK_MODE_FILO) {
            /* Insert before same prio */
            do {
                if (prio <= iter->prio)
                    break;
                iter = container_of(iter->list.next, BottomHalfJob, list);
            } while (iter != first);

            if (iter == first)
                list_add_tail(&bhj->list, &iter->list);
            else
                list_add(&bhj->list, iter->list.prev);
            
        } else {
            /* Insert after same prio */
            do {
                if (prio > iter->prio)
                    break;
                iter = container_of(iter->list.next, BottomHalfJob, list);
            } while (iter != first);

            if (iter == first)
                list_add_tail(&bhj->list, &iter->list);
            else
                list_add(&bhj->list, iter->list.prev);
        }
    }
    
    return bhj;
}

void timer_intr_handler()
{
    jiffies++;
    current->time--;

    if (IS_TIME_JOB_EMPTY)
        return;

    TimeJob *iter = container_of(time_jobs_hdr.next, TimeJob, list);
    TimeJob *next;
    uint32_t del_cnt = 0;

    while (&iter->list != &time_jobs_hdr)
    {
        next = container_of(iter->list.next, TimeJob, list);
        if (iter->duration-- == 0) {
            iter->callback(iter->arg);
            list_del(&iter->list);
            del_cnt++;
        }
        iter = next;
    }
    
    return;
}

void irq_handler()
{
    uint32_t int_src = *(uint32_t *)CORE0_INTERRUPT_SRC;
    BottomHalfJob *bhj = NULL, *prev = NULL;

    /* No more bottom half job */
    if (!bhj_pool_bitmap)
        hangon();
    
    if (int_src & 0b10) {
        disable_timer();
        bhj = add_bhj(timer_intr_handler, NULL, 1);
    } else if (aux_regs->mu_iir & 0b110) {
        disable_uart();
        bhj = add_bhj((void (*)(void*))uart_intr_handler,
                        (void *)(uint64_t)aux_regs->mu_ier, 2);
    } else
        return;

    BottomHalfJob *first = container_of(bhj_hdr.next, BottomHalfJob, list);
    if (bhj != first)
        return;

    while (!IS_BHJ_EMPTY && !bhj->running)
    {
        bhj->running = 1;

        /* Preemptive callback */
        enable_intr();
        bhj->callback(bhj->arg);
        disable_intr();
        
        prev = bhj;
        bhj = container_of(bhj->list.next, BottomHalfJob, list);
        
        list_del(&prev->list);
        __free_bhj(prev);
    }
}
