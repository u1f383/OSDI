#include <linux/sched.h>
#include <kernel/kernel.h>m
#include <linux/mm.h>
#include <interrupt/irq.h>
#include <lib/printf.h>
#include <util.h>
#include <types.h>

/* Thread 0 is main thread */
TaskStruct tasks[ TASK_NUM ] = {0};
uint8_t task_count = 0;

static inline uint8_t get_task_count()
{
    return task_count;
}
static inline bool is_task_full()
{
    return task_count == TASK_NUM;
}

void try_context_switch()
{
    TaskStruct *curr = get_current();
    if (curr->status == RUNNING)
        return;

    /* Context switch */
    if (get_task_count() != 0)
        context_switch(&curr);
        
    curr->time = TIME_SLOT;
}

void __switch_to(TaskStruct **curr)
{
    TaskStruct *next = container_of((*curr)->list.next, TaskStruct, list);
    next->status = RUNNING;
    write_sysreg(tpidr_el1, next);

    *curr = next;
}

void context_switch(TaskStruct **curr)
{
    __switch_to(curr);
}

void main_thread_init()
{
    tasks[0].status = RUNNING;
    tasks[0].prio = 1;
    tasks[0].time = TIME_SLOT;
    LIST_INIT(tasks[0].list);
    task_count++;

    write_sysreg(tpidr_el1, &tasks[0]);
    set_timer(tasks[0].time);
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

uint32_t create_kern_task(void(*func)(), void *arg)
{
    if (is_task_full())
        return 1;

    int i = 0;
    for (; i < TASK_NUM; i++)
        if (tasks[i].status == EMPTY)
            break;

    tasks[i].status = STOPPED;
    tasks[i].prio = 2;

    ThreadInfo *thread_info = &tasks[i].thread_info;
    memset(thread_info, 0, sizeof(ThreadInfo));

    thread_info->x0 = func;
    thread_info->x1 = arg;
    thread_info->pc = thread_trampoline;
    thread_info->sp = (uint64_t) kmalloc(THREAD_STACK_SIZE);
    thread_info->sp += THREAD_STACK_SIZE - 8;
    thread_info->fp = thread_info->sp;
    thread_info->spsr_el1 = 0x5;

    list_add_tail(&tasks[i].list, &tasks[0].list);
    return 0;
}

uint32_t create_user_task(void(*prog)())
{
    if (is_task_full())
        return 1;

    int i = 0;
    for (; i < TASK_NUM; i++)
        if (tasks[i].status == EMPTY)
            break;

    tasks[i].status = STOPPED;
    tasks[i].prio = 2;

    ThreadInfo *thread_info = &tasks[i].thread_info;
    memset(thread_info, 0, sizeof(ThreadInfo));

    thread_info->x0 = prog;
    thread_info->pc = from_el1_to_el0;
    thread_info->sp = (uint64_t) kmalloc(THREAD_STACK_SIZE);
    thread_info->sp += THREAD_STACK_SIZE - 8;
    thread_info->x1 = thread_info->sp;
    thread_info->fp = thread_info->sp;
    thread_info->spsr_el1 = 0x5;

    list_add_tail(&tasks[i].list, &tasks[0].list);
    return 0;
}

void idle()
{
    while (1) {
        delay(0x100000);
        printf("%p\r\n", get_current());
    }
}