#include <sched.h>
#include <mm.h>
#include <signal.h>
#include <irq.h>
#include <printf.h>
#include <util.h>
#include <types.h>
#include <initramfs.h>
#include <fs.h>

static inline void update_timer()
{
    write_sysreg(cntp_tval_el0, TIME_UNIT);
    enable_timer();
}

pgd_t *next_pgd;

/* Thread 0 is main thread */
TaskStruct *main_task;
TaskQueue rq, eq;

static uint64_t _currpid = 1;

void task_queue_init()
{
    /**
     * Because run queue is used in schedule(),
     * we will disable the interrupt instead of locking the rq
     */
    rq.list = LIST_HEAD_INIT(rq.list);
    rq.len = rq.lock = 0;

    eq.list = LIST_HEAD_INIT(eq.list);
    eq.len = eq.lock = 0;
}

TaskStruct *new_task()
{
    TaskStruct *task = kmalloc(sizeof(TaskStruct));
    memset(task, 0, sizeof(TaskStruct));
    LIST_INIT(task->list);
    task->time = 1;
    return task;
}

void schedule()
{
    if (rq.len == 1)
        return;

    disable_intr();

    /* Pick next task */
    TaskStruct *next = container_of(current->list.next, TaskStruct, list);
    while (&next->list == &rq.list || next == current)
        next = container_of(next->list.next, TaskStruct, list);

    update_timer();
    switch_to(current, next, virt_to_phys(next->mm->pgd));
}

void try_schedule()
{
    if (current != NULL && current->time == 0) {
        schedule();
        current->time = TIME_SLOT;
    } else {
        update_timer();
    }
}

void main_thread_init()
{
    main_task = new_task();
    main_task->pid = 0;
    main_task->status = RUNNING;
    main_task->prio = 1;
    main_task->user_stack = 0;
    main_task->kern_stack = (void *)kern_end;
    main_task->mm = kmalloc(sizeof(mm_struct));
    main_task->mm->pgd = (pgd_t *)spin_table_start;
    main_task->mm->mmap = NULL;

    LIST_INIT(main_task->list);
    
    disable_intr();
    list_add(&main_task->list, rq.list.next);
    rq.len++;
    enable_intr();

    write_sysreg(tpidr_el1, main_task);
    update_timer();
}

void thread_release(TaskStruct *target, int16_t ec)
{
    /* Main thread cannot be killed */
    if (target == main_task)
        return;

    target->status = EXITED;
    target->exit_code = ec;

    TaskStruct *next = NULL;
    
    disable_intr();
    if (target == current) {
        /* Find the next task */
        next = container_of(current->list.next, TaskStruct, list);
        while (&next->list == &rq.list || next == current)
            next = container_of(next->list.next, TaskStruct, list);
    }

    list_del(&target->list);
    rq.len--;
    enable_intr();

    while (eq.lock);
    eq.lock = 1;

    LIST_INIT(target->list);
    list_add_tail(&target->list, &eq.list);
    eq.len++;
    
    eq.lock = 0;

    if (target == current) {
        disable_intr();
        update_timer();
        next_pgd = (pgd_t *)virt_to_phys(next->mm->pgd);
        switch_to(current, next, virt_to_phys(next->mm->pgd));
    }
    /* Never reach */
}

void thread_trampoline(void(*func)(), void *arg)
{
    func(arg);
    thread_release(current, EXIT_CODE_OK);
}

uint32_t create_kern_task(void(*func)(), void *arg)
{
    TaskStruct *task = new_task();
    ThreadInfo *thread_info = &task->thread_info;
    
    task->mm = kmalloc(sizeof(mm_struct));
    memset(task->mm, 0, sizeof(mm_struct));
    task->mm->pgd = (pgd_t *)spin_table_start;

    task->pid = 0;
    task->status = STOPPED;
    task->prio = 2;
    task->user_stack = 0;

    thread_info->x19 = (uint64_t)func;
    thread_info->x20 = (uint64_t)arg;
    
    thread_info->sp = (uint64_t)kmalloc(THREAD_STACK_SIZE);
    task->kern_stack = (void *)thread_info->sp;
    thread_info->sp += THREAD_STACK_SIZE - 0x10;
    thread_info->fp = thread_info->sp;

    thread_info->lr = (uint64_t)__thread_trampoline;
    
    disable_intr();
    list_add_tail(&task->list, &rq.list);
    rq.len++;
    enable_intr();

    return 0;
}

uint32_t create_user_task(CpioHeader cpio_obj)
{
    TaskStruct *task = new_task();
    ThreadInfo *thread_info = &task->thread_info;
    mm_struct *mm = kmalloc(sizeof(mm_struct));
    
    task->fdt = kmalloc(sizeof(struct fdt_struct));
    memset(task->fdt, 0, sizeof(struct fdt_struct));

    task->workdir = rootfs->root;
    
    memset(mm, 0, sizeof(mm_struct));
    mm->pgd = buddy_alloc(1);
    memset(mm->pgd, 0, PAGE_SIZE);

    task->mm = mm;
    task->pid = _currpid++;
    task->status = STOPPED;
    task->prio = 2;

    task->prog_size = (cpio_obj.c_filesize + 0xfff) & ~0xfff;
    task->prog = buddy_alloc(task->prog_size >> PAGE_SHIFT);
    memcpy(task->prog, cpio_obj.content, cpio_obj.c_filesize);

    mappages(mm->pgd, 0x0, task->prog_size, virt_to_phys(task->prog));
    thread_info->x19 = 0; /* User code at 0x0 */
    
    task->user_stack = buddy_alloc(THREAD_STACK_SIZE >> PAGE_SHIFT);
    memset(task->user_stack, 0, THREAD_STACK_SIZE);
    mappages(mm->pgd, 0xffffffffb000, THREAD_STACK_SIZE, virt_to_phys(task->user_stack));
    thread_info->x20 = 0xffffffffb000 + THREAD_STACK_SIZE - 0x10;

    thread_info->sp = (uint64_t)kmalloc(THREAD_STACK_SIZE);
    task->kern_stack = (void *)thread_info->sp;
    thread_info->sp += THREAD_STACK_SIZE - 0x10;
    thread_info->fp = thread_info->sp;

    thread_info->lr = (uint64_t)from_el1_to_el0;

    disable_intr();
    list_add_tail(&task->list, &rq.list);
    rq.len++;
    enable_intr();

    return 0;
}

void kill_zombies()
{
    if (IS_EQ_EMPTY)
        return;

    while (eq.lock);
    eq.lock = 1;
    
    TaskStruct *iter = container_of(eq.list.next, TaskStruct, list);
    TaskStruct *prev;

    while (eq.len)
    {
        if (iter->signal != NULL)
        {
            Signal *sig_iter = iter->signal;
            Signal *sig_next;
            do {
                sig_next = container_of(sig_iter->list.next, Signal, list);
                kfree(sig_iter);
                sig_iter = sig_next;
            } while (sig_iter != sig_next);
        }

        prev = iter;
        iter = container_of(iter->list.next, TaskStruct, list);

        list_del(&prev->list);
        if (prev->user_stack)
            kfree(prev->user_stack);
        kfree(prev->kern_stack);

        if ((uint64_t)prev->mm->pgd != spin_table_start)
            release_pgtable(prev->mm->pgd, 0);

        if (prev->mm->mmap) {
            /* TODO */
        }
        kfree(prev->mm);

        if (prev->fdt)
            kfree(prev->fdt);
        
        if (prev->prog)
            kfree(prev->prog);
        
        kfree(prev);
        eq.len--;
    }

    eq.lock = 0;
}

void idle()
{
    while (1) {
        kill_zombies();
        schedule();
    }
}

int32_t svc_fork()
{
    TrapFrame *tf = (void *)read_normreg(x8);
    TaskStruct *task = new_task();
    mm_struct *mm = kmalloc(sizeof(mm_struct));

    task->workdir = current->workdir;

    memset(mm, 0, sizeof(mm_struct));
    mm->pgd = buddy_alloc(1);
    memset(mm->pgd, 0, PAGE_SIZE);
    task->mm = mm;

    task->pid = _currpid++;
    task->prio = current->prio;
    task->status = RUNNING;

    task->prog_size = current->prog_size;
    task->prog = buddy_alloc(current->prog_size >> PAGE_SHIFT);
    memcpy(task->prog, current->prog, current->prog_size);
    mappages(mm->pgd, 0x0, task->prog_size, virt_to_phys(task->prog));

    task->user_stack = buddy_alloc(THREAD_STACK_SIZE >> PAGE_SHIFT);
    mappages(mm->pgd, 0xffffffffb000, THREAD_STACK_SIZE, virt_to_phys(task->user_stack));

    task->kern_stack = kmalloc(THREAD_STACK_SIZE);
    memcpy(task->user_stack, current->user_stack, THREAD_STACK_SIZE);
    memcpy(task->kern_stack, current->kern_stack, THREAD_STACK_SIZE);

    int64_t ufp_offset = tf->x29 - (uint64_t)current->user_stack;
    uint64_t tf_offset = (uint64_t)tf - (uint64_t)current->kern_stack;
    task->thread_info.lr = (uint64_t)fork_trampoline;
    task->thread_info.sp = (uint64_t)task->kern_stack + tf_offset; // new trap frame
    task->thread_info.fp = (uint64_t)task->kern_stack + ufp_offset;
    task->thread_info.x19 = tf->x30;
    task->thread_info.x20 = tf->sp_el0;

    if (current->signal != NULL)
    {
        Signal *iter = current->signal;
        do {
            Signal *new_sig = new_signal(iter->signo, iter->handler);
            if (task->signal == NULL) {
                task->signal = new_sig;
            } else {
                list_add_tail(&new_sig->list, &task->signal->list);
            }
            iter = container_of(iter->list.next, Signal, list);
        } while (iter != current->signal);
    }
    
    disable_intr();
    list_add_tail(&task->list, &rq.list);
    rq.len++;
    enable_intr();

    tf->x0 = task->pid;
    return task->pid;
}

int32_t svc_exec(const char *name, char *const argv[])
{
    TrapFrame *tf = (void *)read_normreg(x8);
    CpioHeader cpio_obj;
    
    if (cpio_find_file(name, &cpio_obj) != 0)
        return -1;

    /* TODO: reallocate prog memory */

    current->workdir = rootfs->root;
    current->pid = _currpid++;
    tf->x30 = (uint64_t)current->prog;
    tf->sp_el0 = (tf->sp_el0 & (THREAD_STACK_SIZE - 1)) + THREAD_STACK_SIZE - 0x10;

    return 0;
}