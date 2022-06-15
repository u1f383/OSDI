#ifndef _SIGNAL_H_
#define _SIGNAL_H_

#include <types.h>
#include <list.h>

typedef struct _Signal {
    int32_t signo;
    void (*handler)();
    struct list_head list;
} Signal;

typedef struct _SignalCtx {
    void *tf;
} SignalCtx;

#include <sched.h>

Signal *new_signal(int SIGNAL, void (*handler)());
SignalCtx *new_signal_ctx(void *tf);
void ignore(int pid);
void sigctx_update(void *trap_frame, void (*handler)());
void try_signal_handle(void *trap_frame);

void svc_kill(int pid);
void svc_sigreturn();
void svc_signal(int SIGNAL, void (*handler)());
void svc_sigkill(int pid, int SIGNAL);

extern void (*default_sighand[])(int);

#define SIGNAL_NUM (sizeof(default_sighand) / sizeof(default_sighand[0]))

#endif /* _SIGNAL_H_ */