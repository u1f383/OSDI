#include <linux/sched.h>
#include <interrupt/irq.h>
#include <gpio/uart.h>
#include <types.h>
#include <util.h>
#include <lib/printf.h>
#include <linux/mm.h>

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


struct list_head time_jobs_hdr = LIST_HEAD_INIT(time_jobs_hdr);
struct list_head bhj_hdr = LIST_HEAD_INIT(bhj_hdr);

bool first_exception = 1;

#define is_time_job_empty() (time_jobs_hdr.next == &time_jobs_hdr)
#define is_bhj_empty() (bhj_hdr.next == &bhj_hdr)

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

BottomHalfJob *new_bhj(void (*callback)(void*), void *arg, int32_t prio)
{
    BottomHalfJob *bhj = kmalloc(sizeof(BottomHalfJob));
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

    if (is_time_job_empty()) {
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

    if (is_bhj_empty()) {
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

    if (is_time_job_empty())
        return;

    TimeJob *iter = container_of(time_jobs_hdr.next, TimeJob, list);
    TimeJob *next;
    uint32_t del_cnt = 0;

    while (iter != &time_jobs_hdr)
    {
        next = container_of(iter->list.next, TimeJob, list);
        if (iter->duration-- == 0)
        {
            iter->callback(iter->arg);
            list_del(iter);
            del_cnt++;
        }
        iter = next;
    }
    
    return;
}

void irq_handler()
{
    uint32_t int_src = *(uint32_t *) CORE0_INTERRUPT_SRC;
    BottomHalfJob *bhj = NULL, *prev = NULL;
    
    /* Timer handling - check if CNTPNSIRQ interrupt bit is set */
    if (int_src & 0b10)
    {
        bhj = add_bhj(timer_intr_handler, NULL, 1);
        disable_timer();
    }
    else if (aux_regs->mu_iir & 0b110)
    {
        /* Mask UART interrupt */
        bhj = add_bhj(uart_intr_handler, aux_regs->mu_ier, 2);
        disable_uart();
    }
    else
        return;

    BottomHalfJob *first = container_of(bhj_hdr.next, BottomHalfJob, list);
    if (bhj != first)
        return;

    while (!is_bhj_empty() && !bhj->running)
    {
        bhj->running = 1;

        /* Preemptive callback */
        enable_intr();
        bhj->callback(bhj->arg);
        disable_intr();
        
        prev = bhj;
        bhj = container_of(bhj->list.next, BottomHalfJob, list);
        
        list_del(&prev->list);
    }
}
