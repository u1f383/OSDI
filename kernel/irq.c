#include <kernel/irq.h>
#include <gpio/uart.h>
#include <types.h>
#include <util.h>

#define TIME_JOB_NUM 0x10

typedef struct _TimeJob
{
    uint32_t duration;
    void (*callback)(void*);
    void *arg;
} TimeJob;

/* TODO: implement in RB tree */
TimeJob time_jobs[TIME_JOB_NUM];
uint32_t time_jobs_top;

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
    } else
        disble_timer();
}

void irq_handler()
{
    uint32_t int_src = *(uint32_t *) CORE0_INTERRUPT_SRC;

    /* Timer handling - check if CNTPNSIRQ interrupt bit is set */
    if (int_src & 0b10)
        timer_intr_handler();
    else if (aux_regs->mu_iir & 0b110)
        uart_intr_handler();
}
