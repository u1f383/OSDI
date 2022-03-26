#include <interrupt/irq.h>
#include <gpio/uart.h>
#include <types.h>
#include <util.h>

#define TIME_JOB_NUM 0x10
#define TASK_ENTRY_NUM 0x10

typedef struct _TimeJob
{
    uint32_t duration;
    void (*callback)(void*);
    void *arg;
} TimeJob;
/* TODO: implement in RB tree */
TimeJob time_jobs[TIME_JOB_NUM];
uint32_t time_jobs_top;

#define TASK_MODE_LILO 0b0
#define TASK_MODE_FILO 0b1
#define TASK_MODE TASK_MODE_LILO

typedef struct _TaskEntry
{
    int32_t prio;
    void (*callback)(void*);
    void *arg;
} TaskEntry;
TaskEntry task_entries[TASK_ENTRY_NUM];
uint32_t task_entries_cur = 0;
uint32_t task_entries_top = 0;
uint64_t task_first = 1;

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

static inline void disble_timer()
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
    if (TASK_MODE == TASK_MODE_FILO)
        return task_entries_top == TASK_ENTRY_NUM;
    
    /* TASK_MODE_LILO */
    return task_entries_top == task_entries_cur - 1 ||
            task_entries_top == task_entries_cur - TASK_ENTRY_NUM + 1;
}

static inline int is_task_empty()
{
    if (TASK_MODE == TASK_MODE_FILO)
        return task_entries_top == 0;
    
    /* TASK_MODE_LILO */
    return task_entries_top == task_entries_cur;
}

void add_task(void (*callback)(void*), void *arg, int32_t prio)
{
    if (is_task_fill())
        return;

    if (TASK_MODE == TASK_MODE_FILO)
        task_entries[ task_entries_top++ ] = (TaskEntry) { .callback=callback, .arg=arg, .prio=prio };
    else
    {
        task_entries[ task_entries_top ] = (TaskEntry) { .callback=callback, .arg=arg, .prio=prio };
        task_entries_top = (task_entries_top+1) % TASK_ENTRY_NUM;
    }
}

void do_task()
{
    if (is_task_empty())
        return;

    while (!is_task_empty())
    {
        if (TASK_MODE == TASK_MODE_FILO)
        {
            task_entries[ task_entries_cur ].callback(task_entries[ task_entries_cur ].arg);
            task_entries_cur = (task_entries_cur+1) % TASK_ENTRY_NUM;
        }
        else
        {
            task_entries[ task_entries_top-1 ].callback(task_entries[ task_entries_top-1 ].arg);
            task_entries_top--;
        }
    }
}

void timer_intr_handler()
{
    if (is_time_job_empty())
        return;

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
    uint32_t _loc_task_first;
    
    /* Timer handling - check if CNTPNSIRQ interrupt bit is set */
    if (int_src & 0b10)
    {
        disble_timer();
        add_task(timer_intr_handler, NULL, 0);
    }
    else if (aux_regs->mu_iir & 0b110)
    {
        /* Mask UART interrupt */
        add_task(uart_intr_handler, aux_regs->mu_ier, 2);
        set_value(aux_regs->mu_ier, 0, AUXMUIER_Enable_receive_interrupts_BIT, AUXMUIER_RESERVED_BIT);
    }

    if (task_first) {
        _loc_task_first = 1;
        task_first = 0;
    } else {
        _loc_task_first = 0;
    }
    /* Enable interrupt */
    __asm__ volatile ("msr DAIFClr, 0xf");

    if (_loc_task_first)
    {
        while (!is_task_empty())
            do_task();
        task_first = 1;
    }
}
