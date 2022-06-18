#include <sched.h>
#include <mm.h>
#include <vm.h>
#include <signal.h>
#include <irq.h>
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
    disable_intr();

    if (rq.len == 1) {
        enable_intr();
        return;
    }

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
        // update_timer();
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

    thread_info->x19 = (uint64_t)func;
    thread_info->x20 = (uint64_t)arg;
    
    thread_info->sp = (uint64_t)buddy_alloc(4);
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

uint32_t create_user_task(const char *pathname)
{
    char component_name[16];
    struct vnode *prev, *vnode;
    
    if (__vfs_lookup(pathname, component_name, &prev, &vnode) != 0)
        return 1;

    if (vnode == NULL)
        return 1;

    TaskStruct *task = new_task();
    ThreadInfo *thread_info = &task->thread_info;
    mm_struct *mm;
    vm_area_struct *vma;
    
    task->workdir = rootfs->root;
    
    mm = kmalloc(sizeof(mm_struct));
    memset(mm, 0, sizeof(mm_struct));

    task->fdt = kmalloc(sizeof(struct fdt_struct));
    memset(task->fdt, 0, sizeof(struct fdt_struct));
    
    mm->pgd = buddy_alloc(1);
    memset(mm->pgd, 0, PAGE_SIZE);

    task->mm = mm;
    task->pid = _currpid++;
    task->status = STOPPED;
    task->prio = 2;

    vma = mmap_internal(task->mm, (void *)0, PAGE_ROUNDUP(vnode->size), PROT_READ | PROT_EXEC, 0);
    vma->data = (const char *)vnode->internal.mem;
    thread_info->x19 = 0; /* User code at 0x0 */
    
    vma = mmap_internal(task->mm, (void *)USER_THREAD_BASE_ADDR, THREAD_STACK_SIZE, PROT_READ | PROT_WRITE, 0);
    thread_info->x20 = USER_THREAD_BASE_ADDR + THREAD_STACK_SIZE - 0x10;

    /**
     * Kernel space need not demand paging
     */
    thread_info->sp = (uint64_t)buddy_alloc(4);
    task->kern_stack = (void *)thread_info->sp;
    thread_info->sp += THREAD_STACK_SIZE - 0x10;
    thread_info->fp = thread_info->sp;

    thread_info->lr = (uint64_t)from_el1_to_el0;

    struct file *stdin, *stdout, *stderr;

    if (__vfs_open_wrapper("/dev/uart", 0, &stdin)  != 0 ||
        __vfs_open_wrapper("/dev/uart", 0, &stdout) != 0 ||
        __vfs_open_wrapper("/dev/uart", 0, &stderr) != 0)
        return -1;
    
    task->fdt->files[0] = stdin;
    task->fdt->files[1] = stdout;
    task->fdt->files[2] = stderr;

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
        kfree(prev->kern_stack);

        if ((uint64_t)prev->mm->pgd != spin_table_start)
            release_pgtable(prev->mm->pgd, 0);

        if (prev->mm->mmap) {
            /* TODO */
        }
        kfree(prev->mm);

        if (prev->fdt)
            kfree(prev->fdt);
        
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
    mm_struct *mm;

    task->workdir = current->workdir;

    task->fdt = kmalloc(sizeof(struct fdt_struct));
    memset(task->fdt, 0, sizeof(struct fdt_struct));

    for (int i = 0; i < FDT_SIZE; i++) {
        if (current->fdt->files[i] != NULL) {
            task->fdt->files[i] = kmalloc(sizeof(struct file));
            memcpy(task->fdt->files[i], current->fdt->files[i], sizeof(struct file));
        }
    }

    mm = kmalloc(sizeof(mm_struct));
    memset(mm, 0, sizeof(mm_struct));
    dup_vma(current->mm, mm);

    mm->pgd = buddy_alloc(1);
    memset(mm->pgd, 0, PAGE_SIZE);
    dup_pages(current->mm->pgd, mm->pgd, 0);

    task->mm = mm;
    task->pid = _currpid++;
    task->prio = current->prio;
    task->status = RUNNING;

    task->kern_stack = buddy_alloc(4);
    memcpy(task->kern_stack, current->kern_stack, THREAD_STACK_SIZE);

    uint64_t tf_offset = (uint64_t)tf - (uint64_t)current->kern_stack;
    task->thread_info.lr = (uint64_t)fork_trampoline;
    task->thread_info.sp = (uint64_t)task->kern_stack + tf_offset;
    task->thread_info.fp = current->thread_info.fp;
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
    vm_area_struct *vma = find_vma(current->mm, 0);

    if (vma == NULL || vma->data == NULL)
        return -1;

    char component_name[16];
    struct vnode *prev, *vnode;
    
    if (__vfs_lookup(name, component_name, &prev, &vnode) != 0)
        return 1;

    if (vnode == NULL)
        return 1;
    
    release_vma(current->mm);
    vma = mmap_internal(current->mm, (void *)0, PAGE_ROUNDUP(vnode->size), PROT_READ | PROT_EXEC, 0);
    vma = mmap_internal(current->mm, (void *)USER_THREAD_BASE_ADDR, THREAD_STACK_SIZE, PROT_READ | PROT_WRITE, 0);

    vma->data = (const char *)vnode->internal.mem;
    current->workdir = rootfs->root;
    current->pid = _currpid++;

    tf->elr_el1 = 0;
    tf->sp_el0 = USER_THREAD_BASE_ADDR + THREAD_STACK_SIZE - 0x10;

    return 0;
}