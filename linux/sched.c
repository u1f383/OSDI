#include <linux/sched.h>
#include <kernel/kernel.h>m
#include <linux/mm.h>
#include <interrupt/irq.h>
#include <lib/printf.h>
#include <util.h>
#include <types.h>

/* Thread 0 is main thread */
TaskStruct tasks[ TASK_NUM ] = {0};
TaskStruct *rq = NULLPTR, \
           *wq = NULLPTR, \
           *eq = NULLPTR;
TaskStruct *main_task = &tasks[0];
uint8_t rq_len = 0, \
        wq_len = 0, \
        eq_len = 0;
uint64_t _currpid = 1;

static inline uint8_t get_rq_count()
{
    return rq_len;
}

static inline uint8_t get_wq_count()
{
    return wq_len;
}

static inline uint8_t get_eq_count()
{
    return eq_len;
}

static inline uint16_t get_task_count()
{
    return rq_len + wq_len + eq_len;
}

static inline bool is_task_full()
{
    return get_task_count() == TASK_NUM;
}

uint32_t __fork()
{
    if (is_task_full())
        return 1;

    int i = 0;
    for (; i < TASK_NUM; i++)
        if (tasks[i].status == EMPTY)
            break;

    uint64_t sp_el0 = read_sysreg(sp_el0);
    TaskStruct *curr = current;

    memcpy(&tasks[i], curr, sizeof(TaskStruct));
    tasks[i].pid = _currpid++;
    tasks[i].time = TIME_SLOT;

    int64_t usp_offset = sp_el0 - curr->user_stack;
    int64_t ufp_offset = curr->thread_info.fp - curr->user_stack;

    ThreadInfo *new_thread_info = &tasks[i].thread_info;
    new_thread_info->x1 = (uint64_t) kmalloc(THREAD_STACK_SIZE);
    tasks[i].user_stack = new_thread_info->x1;
    new_thread_info->fp = new_thread_info->x1;

    new_thread_info->x1 += usp_offset;
    new_thread_info->fp += ufp_offset;

    new_thread_info->x8 = curr->thread_info.x1;

    new_thread_info->sp = (uint64_t) kmalloc(THREAD_STACK_SIZE);
    tasks[i].kern_stack = new_thread_info->sp;
    
    new_thread_info->x0 = curr->thread_info.pc;
    new_thread_info->pc = ret_from_fork;
    new_thread_info->spsr_el1 = 0x5;

    memcpy(tasks[i].user_stack, curr->user_stack, THREAD_STACK_SIZE);

    list_add_tail(&tasks[i].list, &rq->list);

    return tasks[i].pid;
}

uint32_t __exec(void(*prog)(), char *const argv[])
{
    TaskStruct *curr = current;

    curr->pid = _currpid++;
    curr->thread_info.pc = prog;
    curr->thread_info.sp = curr->user_stack;
    curr->thread_info.sp += THREAD_STACK_SIZE - 8;

    char **stack = (char **) curr->thread_info.sp;
    int argc = 0;
    for (; argv[argc] != NULL; argc++);

    while (argc-- >= 0) {
        *stack = argv[argc];
        stack--;
    }

    return 0;
}

void try_context_switch()
{
    TaskStruct *curr = current;
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

    (*curr)->status = STOPPED;
    *curr = next;
}

void context_switch(TaskStruct **curr)
{
    __switch_to(curr);
}

void main_thread_init()
{
    memset(&tasks[0], 0, sizeof(ThreadInfo));

    tasks[0].pid = 0;
    tasks[0].status = RUNNING;
    tasks[0].prio = 1;
    tasks[0].time = TIME_SLOT;
    LIST_INIT(tasks[0].list);

    rq = tasks;
    rq_len++;

    write_sysreg(tpidr_el1, &tasks[0]);
    set_timer(tasks[0].time);
    enable_timer();
}

void schedule()
{
    disable_intr();
    return;
}

void thread_release(TaskStruct *target, int16_t ec)
{
    TaskStruct *prev = target;
    
    /* Main thread cannot be killed */
    if (target == main_task)
        return;

    if (target == current)
        context_switch(&target);

    prev->status = EXITED;
    prev->exit_code = ec;
    LIST_INIT(prev->list);
    if (eq == NULLPTR)
        eq = prev;
    else
        list_add_tail(&prev->list, &eq->list);
    
    rq_len--;
    eq_len++;
}

void thread_trampoline(void(*func)(), void *arg)
{
    func(arg);
    thread_release(current, EXIT_CODE_OK);
}

uint32_t create_kern_task(void(*func)(), void *arg)
{
    if (is_task_full())
        return 1;

    int i = 0;
    for (; i < TASK_NUM; i++)
        if (tasks[i].status == EMPTY)
            break;

    tasks[i].pid = 0;
    tasks[i].status = STOPPED;
    tasks[i].prio = 2;

    ThreadInfo *thread_info = &tasks[i].thread_info;
    memset(thread_info, 0, sizeof(ThreadInfo));

    thread_info->x0 = func;
    thread_info->x1 = arg;
    thread_info->pc = thread_trampoline;
    thread_info->sp = (uint64_t) kmalloc(THREAD_STACK_SIZE);
    tasks[i].user_stack = 0;
    tasks[i].kern_stack = thread_info->sp;
    thread_info->sp += THREAD_STACK_SIZE - 8;
    thread_info->fp = thread_info->sp;
    thread_info->spsr_el1 = 0x5;

    list_add_tail(&tasks[i].list, &rq->list);
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

    tasks[i].pid = _currpid++;
    tasks[i].status = STOPPED;
    tasks[i].prio = 2;

    ThreadInfo *thread_info = &tasks[i].thread_info;
    memset(thread_info, 0, sizeof(ThreadInfo));

    thread_info->x0 = prog;
    
    thread_info->x1 = (uint64_t) kmalloc(THREAD_STACK_SIZE);
    tasks[i].user_stack = thread_info->x1;
    thread_info->x1 += THREAD_STACK_SIZE - 8;
    thread_info->fp = thread_info->x1;

    thread_info->sp = (uint64_t) kmalloc(THREAD_STACK_SIZE);
    tasks[i].kern_stack = thread_info->sp;
    thread_info->sp += THREAD_STACK_SIZE - 8;
    
    thread_info->pc = from_el1_to_el0;
    thread_info->spsr_el1 = 0x5;

    list_add_tail(&tasks[i].list, &rq->list);
    return 0;
}

void idle()
{
    while (1) {
        delay(0x100000);
        printf("%p\r\n", get_current());
    }
}