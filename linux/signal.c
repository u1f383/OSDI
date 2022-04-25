#include <linux/signal.h>
#include <linux/mm.h>
#include <linux/sched.h>

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
    sigkill_handler,
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
    signal_ctx->user_stack = 0;
    signal_ctx->tf = kmalloc(sizeof(TrapFrame));
    memcpy(signal_ctx->tf, tf, sizeof(TrapFrame));
    return signal_ctx;
}

void sigkill_handler(int pid)
{
    disable_intr();
    
    TaskStruct *first, *curr;
    /* Traverse run queue */
    if (!is_rq_empty()) {
        first = container_of(rq.next, TaskStruct, list);
        curr = first;
        
        do {
            if (curr->pid == pid) {
                thread_release(curr, EXIT_CODE_KILL);
                return;
            }

            curr = container_of(curr->list.next, TaskStruct, list);
        } while (curr != first);
    }

    /* Traverse wait queue */
    if (!is_wq_empty()) {
        first = container_of(wq.next, TaskStruct, list);
        curr = first;
        
        do {
            if (curr->pid == pid) {
                thread_release(curr, EXIT_CODE_KILL);
                return;
            }

            curr = container_of(curr->list.next, TaskStruct, list);
        } while (curr != first);
    }

    enable_intr();
}

void ignore(int pid)
{
    return;
}