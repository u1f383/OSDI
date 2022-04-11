#include <interrupt/irq.h>
#include <gpio/uart.h>
#include <types.h>
#include <util.h>

#define TIME_JOB_NUM 0x30
#define TASK_ENTRY_NUM 0x10

typedef struct _TimeJob
{
    uint32_t duration;
    void (*callback)(void*);
    void *arg;
} TimeJob;

TimeJob time_jobs[TIME_JOB_NUM] = {0};
uint32_t tj_tail;

TaskEntry task_entries[TASK_ENTRY_NUM];
TaskEntry *te_hdr = NULL;
uint32_t te_cnt = 0;

static inline int is_time_job_fill()
{
    return tj_tail == TIME_JOB_NUM;
}

static inline int is_time_job_empty()
{
    return tj_tail == 0;
}

/* Enable timer for Arm core0 timer IRQ */
static inline void enable_timer()
{
    *(uint32_t *) CORE0_TIMER_IRQ_CTRL = 2;
}

static inline void disable_timer()
{
    *(uint32_t *) CORE0_TIMER_IRQ_CTRL = 0;
}

static inline void disable_intr()
{
    __asm__("msr DAIFSet, 0xf");
}

static inline void enable_intr()
{
    __asm__("msr DAIFClr, 0xf");
}

void add_timer(void(*callback)(void*), void *arg, uint32_t duration)
{
    if (is_time_job_fill() || duration == 0)
        return;
    
    disable_timer();

    uint32_t delta;
    uint32_t frq = read_sysreg(cntfrq_el0);
    uint32_t curr_tval = read_sysreg(cntp_tval_el0);
    
    delta = is_time_job_empty() ? 0 : time_jobs[0].duration - curr_tval;
    duration *= frq; /* Second to frequency */

    for (int i = 0; i < tj_tail; i++)
        time_jobs[i].duration -= delta;

    if (tj_tail != 0 && duration < time_jobs[0].duration)
    {
        /* The time job is fastest */
        time_jobs[ tj_tail ] = time_jobs[0];
        time_jobs[0] = (TimeJob) { .arg=arg, .callback=callback, .duration=duration };
        write_sysreg(cntp_tval_el0, duration);
    }
    else
    {
        time_jobs[ tj_tail ] = (TimeJob) { .arg=arg, .callback=callback, .duration=duration };
        write_sysreg(cntp_tval_el0, time_jobs[0].duration);
    }

    tj_tail++;
    enable_timer();
}

static inline int is_task_fill()
{
    return te_cnt == TASK_ENTRY_NUM;
}

static inline int is_task_empty()
{
    return te_cnt == 0;
}

TaskEntry *get_task_slot()
{
    for (int i = 0; i < TASK_ENTRY_NUM; i++)
        if (task_entries[i].callback == NULL) {
            te_cnt++;
            return &task_entries[i];
        }
    return NULL;
}

void del_task_slot(TaskEntry *te)
{
    te_cnt--;
    te->callback = NULL;
    list_del(&te->list);
}

TaskEntry* add_task(void (*callback)(void*), void *arg, int32_t prio)
{
    if (is_task_fill())
        return 0;

    TaskEntry *te = get_task_slot();
    te->arg = arg;
    te->callback = callback;
    te->prio = prio;
    te->running = 0;
    LIST_INIT(te->list);

    if (te_cnt == 1) {
        te_hdr = te;
    } else {
        TaskEntry *iter = container_of(te_hdr->list.next, TaskEntry, list);
        
        if (TASK_MODE == TASK_MODE_FILO) {
            /* Insert before same prio */
            while (prio > iter->prio && iter != te_hdr)
                iter = container_of(iter->list.next, TaskEntry, list);

            if (prio <= te_hdr->prio)
                te_hdr = te;
            
        } else {
            /* Insert after same prio */
            while (prio >= iter->prio && iter != te_hdr)
                iter = container_of(iter->list.next, TaskEntry, list);

            if (prio < te_hdr->prio)
                te_hdr = te;
        }

        list_add(&te->list, iter->list.prev);
    }

    return te;
}

void timer_intr_handler()
{
    if (is_time_job_empty())
        return;

    uint32_t delta = time_jobs[0].duration;
    int i;

    /* Update rest time and call the callback function */
    for (i = 0; i < tj_tail; i++)
    {
        time_jobs[i].duration -= delta;
        while (time_jobs[i].duration == 0 && i < tj_tail)
        {
            time_jobs[i].callback(time_jobs[i].arg);
            time_jobs[i] = time_jobs[ --tj_tail ];
            time_jobs[i].duration -= delta;
        }
    }

    if (is_time_job_empty())
        return;

    uint32_t min_idx = 0;
    uint32_t min_dura = 0xffffffff;
    TimeJob tmp_tj;
    
    for (i = 0; i < tj_tail; i++)
    {
        if (min_dura > time_jobs[i].duration)
        {
            min_dura = time_jobs[i].duration;
            min_idx = i;
        }
    }
    
    /* Swap idx zero with fastest idx */
    if (min_idx > 0) {
        tmp_tj = time_jobs[ min_idx ];
        time_jobs[ min_idx ] = time_jobs[0];
        time_jobs[0] = tmp_tj;
    }

    write_sysreg(cntp_tval_el0, time_jobs[0].duration);
    enable_timer();
}

void irq_handler()
{
    uint32_t int_src = *(uint32_t *) CORE0_INTERRUPT_SRC;
    TaskEntry *te = NULL, *prev_te = NULL;
    
    /* Timer handling - check if CNTPNSIRQ interrupt bit is set */
    if (int_src & 0b10)
    {
        te = add_task(timer_intr_handler, NULL, 1);
        disable_timer();
    }
    else if (aux_regs->mu_iir & 0b110)
    {
        /* Mask UART interrupt */
        te = add_task(uart_intr_handler, aux_regs->mu_ier, 2);
        disable_uart();
    }
    else
        return;

    if (te != te_hdr)
        return;

    while (te_cnt && !te->running)
    {
        te->running = 1;

        /* Preemptive callback */
        enable_intr();
        te->callback(te->arg);

        disable_intr();
        prev_te = te;
        te = container_of(te->list.next, TaskEntry, list);
        del_task_slot(prev_te);
    }
}
