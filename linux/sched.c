#include <linux/sched.h>
#include <linux/mm.h>
#include <interrupt/irq.h>
#include <lib/printf.h>
#include <util.h>
#include <types.h>

typedef struct _TrapFrame {
        uint64_t x0;
        uint64_t x1;
        uint64_t x2;
        uint64_t x3;
        uint64_t x4;
        uint64_t x5;
        uint64_t x6;
        uint64_t x7;
        uint64_t x8;
        uint64_t x9;
        uint64_t x10;
        uint64_t x11;
        uint64_t x12;
        uint64_t x13;
        uint64_t x14;
        uint64_t x15;
        uint64_t x16;
        uint64_t x17;
        uint64_t x18;
        uint64_t x19;
        uint64_t x20;
        uint64_t x21;
        uint64_t x22;
        uint64_t x23;
        uint64_t x24;
        uint64_t x25;
        uint64_t x26;
        uint64_t x27;
        uint64_t x28;
        uint64_t x29;
        uint64_t x30;
        uint64_t elr_el1;
        uint64_t spsr_el1;
        uint64_t sp_el0;
} TrapFrame;

/* Thread 0 is main thread */
TaskStruct *main_task;

struct list_head rq = LIST_HEAD_INIT(rq), \
                 wq = LIST_HEAD_INIT(wq), \
                 eq = LIST_HEAD_INIT(eq);
uint8_t rq_len = 0, \
        wq_len = 0, \
        eq_len = 0;

uint64_t _currpid = 1;

TaskStruct *new_task()
{
    TaskStruct *task = kmalloc(sizeof(TaskStruct));
    memset(task, 0, sizeof(TaskStruct));
    task->time = 1;
    LIST_INIT(task->list);
    return task;
}

int32_t __fork(void *trap_frame)
{
    TrapFrame *tf = trap_frame;
    TaskStruct *task = new_task();

    task->pid = _currpid++;
    task->prio = current->prio;
    task->status = RUNNING;

    int64_t usp_offset = tf->sp_el0 - current->user_stack;
    int64_t ufp_offset = tf->x29 - current->user_stack;

    task->user_stack = (uint64_t) kmalloc(THREAD_STACK_SIZE);
    task->kern_stack = (uint64_t) kmalloc(THREAD_STACK_SIZE);

    memcpy(task->user_stack, current->user_stack, THREAD_STACK_SIZE);
    memcpy(task->kern_stack, current->kern_stack, THREAD_STACK_SIZE);

    int64_t tf_offset = (uint64_t) tf - current->kern_stack;
    task->thread_info.lr = fork_handler;
    task->thread_info.sp = task->kern_stack + tf_offset; // new trap frame
    task->thread_info.fp = task->kern_stack + ufp_offset;
    task->thread_info.x19 = tf->x30;
    task->thread_info.x20 = task->user_stack + usp_offset;

    rq_len++;
    list_add_tail(&task->list, &rq);

    tf->x0 = task->pid;
    return task->pid;
}

int32_t __exec(void *trap_frame, void(*prog)(), char *const argv[])
{
    TrapFrame *tf = trap_frame;
    char *syscall_img = (char *) 0x8000000;
    memcpy(syscall_img, prog, 246920);

    current->pid = _currpid++;
    
    tf->x30 = syscall_img;
    tf->sp_el0 = current->user_stack;
    tf->sp_el0 += THREAD_STACK_SIZE - 8;

    return 0;
}

void schedule()
{
    TaskStruct *next = container_of(current->list.next, TaskStruct, list);

    while (&next->list == &rq || next == current)
        next = container_of(next->list.next, TaskStruct, list);

    update_timer();
    enable_timer();
    switch_to(current, next);
}

void try_schedule()
{
    if (current->time == 0) {
        if (get_rq_count() > 1)
            schedule();
        current->time = TIME_SLOT;
    }
    update_timer();
    enable_timer();
}

void main_thread_init()
{
    main_task = new_task();
    main_task->pid = 0;
    main_task->status = RUNNING;
    main_task->prio = 1;
    main_task->user_stack = 0;
    main_task->kern_stack = kern_end;
    
    rq_len++;
    list_add(&main_task->list, rq.next);

    write_sysreg(tpidr_el1, main_task);
    update_timer();
    enable_timer();
}

void thread_release(TaskStruct *target, int16_t ec)
{
    /* Main thread cannot be killed */
    if (target == main_task)
        return;

    target->status = EXITED;
    target->exit_code = ec;

    list_del(&target->list);
    LIST_INIT(target->list);
    list_add_tail(&target->list, &eq);
    
    rq_len--;
    eq_len++;

    if (target == current)
        schedule();
}

void thread_trampoline(void(*func)(), void *arg)
{
    func(arg);
    thread_release(current, EXIT_CODE_OK);
}

uint32_t create_kern_task(void(*func)(), void *arg)
{
    disable_intr();

    TaskStruct *task = new_task();
    ThreadInfo *thread_info = &task->thread_info;

    task->pid = 0;
    task->status = STOPPED;
    task->prio = 2;
    task->user_stack = 0;

    thread_info->x19 = func;
    thread_info->x20 = arg;
    
    thread_info->sp = (uint64_t) kmalloc(THREAD_STACK_SIZE);
    task->kern_stack = thread_info->sp;
    thread_info->sp += THREAD_STACK_SIZE - 8;
    thread_info->fp = thread_info->sp;

    thread_info->lr = __thread_trampoline;
    
    rq_len++;
    list_add_tail(&task->list, &rq);

    enable_intr();
    return 0;
}

uint32_t create_user_task(void(*prog)())
{
    disable_intr();

    TaskStruct *task = new_task();
    ThreadInfo *thread_info = &task->thread_info;

    task->pid = _currpid++;
    task->status = STOPPED;
    task->prio = 2;

    thread_info->x19 = prog;
    
    thread_info->x20 = (uint64_t) kmalloc(THREAD_STACK_SIZE);
    task->user_stack = thread_info->x20;
    thread_info->x20 += THREAD_STACK_SIZE - 8;
    

    thread_info->sp = (uint64_t) kmalloc(THREAD_STACK_SIZE);
    task->kern_stack = thread_info->sp;
    thread_info->sp += THREAD_STACK_SIZE - 8;
    thread_info->fp = thread_info->sp;

    thread_info->lr = from_el1_to_el0;

    rq_len++;
    list_add_tail(&task->list, &rq);

    enable_intr();
    return 0;
}

void idle()
{
    while (1) {
        delay(0x100000);
        printf("%p\r\n", get_current());
    }
}