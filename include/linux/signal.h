#ifndef _LINUX_SIGNAL_H_
#define _LINUX_SIGNAL_H_

#include <types.h>
#include <list.h>

typedef struct _Signal {
    int32_t signo;
    void (*handler)();
    struct list_head list;
} Signal;

typedef struct _SignalCtx {
    void *tf;
    void *user_stack;
} SignalCtx;

#include <linux/sched.h>

Signal *new_signal(int SIGNAL, void (*handler)());
SignalCtx *new_signal_ctx(void *tf);
void ignore(int pid);
void sigkill_handler(int pid);

extern void (*default_sighand[])(int);

#define SIGNAL_NUM (sizeof(default_sighand) / sizeof(default_sighand[0]))

#endif /* _LINUX_SIGNAL_H_ */