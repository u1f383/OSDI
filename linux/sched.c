#include <linux/sched.h>
#include <linux/mm.h>
#include <interrupt/irq.h>
#include <lib/printf.h>
#include <util.h>
#include <types.h>

/* Thread 0 is main thread */
ThreadInfo thread_infos[ THREAD_NUM ] = {0};
uint8_t thread_count = 0;

uint8_t get_thread_count()
{
    return thread_count;
}

void try_context_switch()
{
    ThreadInfo *curr = get_current();
    if (curr->status == RUNNING)
        return;

    /* Context switch */
    if (get_thread_count() != 0)
        context_switch(&curr);
        
    curr->time = TIME_SLOT;
}

void context_switch(ThreadInfo **curr)
{
    __switch_to(curr);
}

void __switch_to(ThreadInfo **curr)
{
    ThreadInfo *next = container_of((*curr)->list.next, ThreadInfo, list);
    next->status = RUNNING;
    write_sysreg(tpidr_el1, next);

    *curr = next;
}

void main_thread_init()
{
    thread_infos[0].status = RUNNING;
    thread_infos[0].prio = 1;
    thread_infos[0].time = TIME_SLOT;
    LIST_INIT(thread_infos[0].list);
    thread_count++;

    write_sysreg(tpidr_el1, &thread_infos[0]);
    set_timer(thread_infos[0].time);
    enable_timer();
}

void schedule()
{
    disable_intr();
    return;
}

void thread_release()
{
    
}

void thread_trampoline(void(*func)(), void *arg)
{
    func(arg);
    while (1);
    // thread_release();
}

uint32_t thread_create(void(*func)(), void *arg)
{
    if (thread_count == THREAD_NUM)
        return 1;

    int i = 0;
    for (; i < THREAD_NUM; i++)
        if (thread_infos[i].status == EMPTY)
            break;

    thread_infos[i].status = STOPPED;
    thread_infos[i].prio = 2;

    thread_infos[i].x0 = func;
    thread_infos[i].x1 = arg;
    thread_infos[i].pc = thread_trampoline;
    thread_infos[i].sp = (uint64_t) kmalloc(THREAD_STACK_SIZE);
    thread_infos[i].sp += THREAD_STACK_SIZE - 8;
    thread_infos[i].fp = thread_infos[i].sp;
    thread_infos[i].spsr_el1 = 0x20000005;

    list_add_tail(&thread_infos[i].list, &thread_infos[0].list);
    return 0;
}

void idle()
{
    while (1) {
        delay(0x100000);
        printf("%p\r\n", get_current());
    }
}