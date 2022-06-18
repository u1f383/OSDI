#include <signal.h>
#include <mm.h>
#include <sched.h>

void (*default_sighand[])(int) = \
{
    ignore,
    ignore,
    ignore,
    ignore,
    ignore,
    ignore,
    ignore,
    ignore,
    svc_kill,
};

Signal *new_signal(int SIGNAL, void (*handler)())
{
    Signal *signal = kmalloc(sizeof(Signal));
    signal->signo = SIGNAL;
    signal->handler = handler;
    LIST_INIT(signal->list);
    return signal;
}

SignalCtx *new_signal_ctx(void *tf)
{
    SignalCtx *signal_ctx = kmalloc(sizeof(SignalCtx));
    signal_ctx->tf = kmalloc(sizeof(TrapFrame));
    memcpy(signal_ctx->tf, tf, sizeof(TrapFrame));
    return signal_ctx;
}

void svc_kill(int pid)
{
    TaskStruct *first, *curr;

    if (!IS_RQ_EMPTY) {
        first = container_of(rq.list.next, TaskStruct, list);
        curr = first;
        
        do {
            if (curr->pid == pid) {
                thread_release(curr, EXIT_CODE_KILL);
                return;
            }

            curr = container_of(curr->list.next, TaskStruct, list);
        } while (curr != first);
    }
}

void ignore(int pid)
{
    return;
}

void sigctx_update(void *trap_frame, void (*handler)())
{
    TrapFrame *tf = trap_frame;
    SignalCtx *signal_ctx = new_signal_ctx(tf);
    
    current->signal_ctx = signal_ctx;
    
    tf->x30 = (uint64_t)call_sigreturn;
    tf->elr_el1 = (uint64_t)handler;
    tf->sp_el0 -= 0x100;
}

#define SPSR_MODE_EL0 0b0000
void try_signal_handle(void *trap_frame)
{
    TrapFrame *tf = (TrapFrame *)trap_frame;
    uint32_t mode = tf->spsr_el1 & 0b1111;
    
    if (current->signal_queue && mode == SPSR_MODE_EL0)
    {
        uint32_t signo = current->signal_queue;
        current->signal_queue = 0;
        
        if (current->signal == NULL) {
            default_sighand[signo - 1](current->pid);
        } else {
            Signal *iter = current->signal;
            do {
                if (iter->signo == signo) {
                    sigctx_update(trap_frame, iter->handler);
                    return;
                }
                iter = container_of(iter->list.next, Signal, list);
            } while (iter != current->signal);
            default_sighand[signo - 1](current->pid);
        }
    }
}

void svc_sigreturn()
{
    memcpy((void *)read_normreg(x8), current->signal_ctx->tf, sizeof(TrapFrame));
    kfree(current->signal_ctx->tf);
    kfree(current->signal_ctx);
    current->signal_ctx = NULL;
}

void svc_signal(int SIGNAL, void (*handler)())
{
    Signal *signal = new_signal(SIGNAL, handler);
    
    if (current->signal == NULL)
        current->signal = signal;
    else
        list_add(&signal->list, &current->signal->list);
}

void svc_sigkill(int pid, int SIGNAL)
{
     TaskStruct *first, *curr = NULL;

    if (!IS_RQ_EMPTY) {
        first = container_of(rq.list.next, TaskStruct, list);
        curr = first;
        do {
            if (curr->pid == pid) {
                if (curr->signal_queue == 0)
                    /* Send signal if there is not pending signal */
                    curr->signal_queue = SIGNAL;
                return;
            }
            curr = container_of(curr->list.next, TaskStruct, list);
        } while (curr != first);
    }
}