#include <interrupt/irq.h>
#include <gpio/uart.h>
#include <list.h>
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
/* TODO: implement in RB tree */
TimeJob time_jobs[TIME_JOB_NUM] = {0};
uint32_t time_jobs_top;

#define TASK_MODE_LILO 0b0
#define TASK_MODE_FILO 0b1
#define TASK_MODE TASK_MODE_LILO

typedef struct _TaskEntry
{
    int32_t prio;
    void (*callback)(void*);
    void *arg;
    struct list_head list;
} TaskEntry;
TaskEntry task_entries[TASK_ENTRY_NUM];
TaskEntry *te_hdr = NULL;
uint32_t task_entries_cnt = 0;
uint64_t progress_taskq = 0;

static inline int is_time_job_fill()
{
    return time_jobs_top == TIME_JOB_NUM;
}

static inline int is_time_job_empty()
{
    return time_jobs_top == 0;
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

void add_timer(void(*callback)(void*), void *arg, uint32_t duration)
{
    if (is_time_job_fill())
        return;

    uint32_t delta;
    uint32_t frq = read_sysreg(cntfrq_el0);
    uint32_t curr_tval = read_sysreg(cntp_tval_el0);
    
    delta = is_time_job_empty() ? 0 : time_jobs[0].duration - curr_tval;

    /**
     * Second to frequency
     * ! May be overflow
     */
    duration *= frq;

    __asm__("msr DAIFSet, 0xf");
    for (int i = 0; i < time_jobs_top; i++)
        time_jobs[i].duration -= delta;

    if (time_jobs_top != 0 && duration < time_jobs[0].duration)
    {
        time_jobs[ time_jobs_top ] = time_jobs[0];
        time_jobs[0] = (TimeJob) { .arg=arg, .callback=callback, .duration=duration };
        write_sysreg(cntp_tval_el0, duration);
    }
    else
    {
        time_jobs[ time_jobs_top ] = (TimeJob) { .arg=arg, .callback=callback, .duration=duration };
        if (time_jobs_top == 0)
        {
            enable_timer();
            write_sysreg(cntp_tval_el0, duration);
        }
    }
    time_jobs_top++;
    __asm__("msr DAIFClr, 0xf");
}

static inline int is_task_fill()
{
    return task_entries_cnt == TASK_ENTRY_NUM;
}

static inline int is_task_empty()
{
    return task_entries_cnt == 0;
}

TaskEntry *get_task_slot()
{
    for (int i = 0; i < TASK_ENTRY_NUM; i++)
        if (task_entries[i].callback == NULL) {
            task_entries_cnt++;
            return &task_entries[i];
        }
    return NULL;
}

void del_task_slot(TaskEntry *te)
{
    task_entries_cnt--;
    te->callback = NULL;
    list_del(&te->list);
}

int add_task(void (*callback)(void*), void *arg, int32_t prio)
{
    if (is_task_fill())
        return 0;

    TaskEntry *te = get_task_slot();
    te->arg = arg;
    te->callback = callback;
    te->prio = prio;
    LIST_INIT(te->list);

    if (task_entries_cnt == 1)
        te_hdr = te;
    else {
        TaskEntry *te_iter = container_of(te_hdr->list.next, TaskEntry, list);
        if (TASK_MODE == TASK_MODE_FILO)
            while (te_iter->prio < prio && te_iter != te_hdr)
                te_iter = container_of(te_iter->list.next, TaskEntry, list);
        else
            while (te_iter->prio <= prio && te_iter != te_hdr)
                te_iter = container_of(te_iter->list.next, TaskEntry, list);

        list_add(&te->list, te_iter->list.prev);
        if (te_iter == te_hdr)
            te_hdr = te;
    }
    return (te_hdr == te);
}

void do_task()
{
    if (is_task_empty())
        return;

    TaskEntry *prev_hdr;

    while (1)
    {
        __asm__("msr DAIFSet, 0xf");
        if (!task_entries_cnt)
            break;
        __asm__("msr DAIFClr, 0xf");
        
        te_hdr->callback(te_hdr->arg);

        __asm__("msr DAIFSet, 0xf");
        prev_hdr = te_hdr;
        te_hdr = container_of(te_hdr->list.next, TaskEntry, list);
        del_task_slot(prev_hdr);
        __asm__("msr DAIFClr, 0xf");
    }
    __asm__("msr DAIFClr, 0xf");
}

void timer_intr_handler()
{
    if (is_time_job_empty())
        return;

    disable_timer();

    uint32_t delta = time_jobs[0].duration;
    int i = 0;
    for (; i < time_jobs_top; i++)
    {
        time_jobs[i].duration -= delta;
        while (time_jobs[i].duration == 0 && i < time_jobs_top)
        {
            time_jobs[i].callback(time_jobs[i].arg);
            time_jobs[i] = time_jobs[ --time_jobs_top ];
            time_jobs[i].duration -= delta;
        }
    }

    if (!is_time_job_empty())
    {
        uint32_t min_idx = -1;
        uint32_t min_dura = 0xffffffff;
        
        TimeJob tmp_tjob;
        for (i = 0; i < time_jobs_top; i++)
        {
            if (min_dura > time_jobs[i].duration)
            {
                min_dura = time_jobs[i].duration;
                min_idx = i;
            }
        }
        /* Swap idx zero with top idx */
        tmp_tjob = time_jobs[ min_idx ];
        time_jobs[ min_idx ] = time_jobs[0];
        time_jobs[0] = tmp_tjob;

        write_sysreg(cntp_tval_el0, time_jobs[0].duration);
        enable_timer();
    }
}

void irq_handler()
{
    uint32_t int_src = *(uint32_t *) CORE0_INTERRUPT_SRC;
    uint32_t highest_task = 0;
    
    /* Timer handling - check if CNTPNSIRQ interrupt bit is set */
    if (int_src & 0b10)
    {
        highest_task = add_task(timer_intr_handler, NULL, 0);
        disable_timer();
    }
    else if (aux_regs->mu_iir & 0b110) // uart --> GPU --> core0
    {
        /* Mask UART interrupt */
        highest_task = add_task(uart_intr_handler, aux_regs->mu_ier, 2);
        set_value(aux_regs->mu_ier, 0, AUXMUIER_Enable_receive_interrupts_BIT, AUXMUIER_RESERVED_BIT);
    }

    /* Enable interrupt */
    if (!progress_taskq)
    {
        progress_taskq = 1;
        __asm__("msr DAIFClr, 0xf");
        do_task();
        progress_taskq = 0;
    }
    else if (highest_task)
    {
        __asm__("msr DAIFClr, 0xf");
        te_hdr->callback(te_hdr->arg);
        __asm__("msr DAIFSet, 0xf");
        te_hdr = container_of(te_hdr->list.next, TaskEntry, list);
        __asm__("msr DAIFClr, 0xf");        
    }
}
