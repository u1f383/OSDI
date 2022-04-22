#ifndef _LINUX_SCHED_
#define _LINUX_SCHED_

#include <types.h>
#include <list.h>
#include <interrupt/irq.h>

#define THREAD_STACK_SIZE 0x2000
#define current get_current()

typedef struct _ThreadInfo {
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
    uint64_t fp;
    uint64_t lr;
    uint64_t sp;
} ThreadInfo;

typedef struct _TaskStruct {
    ThreadInfo thread_info;
    int32_t pid;

    int16_t status;
    #define EMPTY   (0)
    #define STOPPED (1<<0)
    #define RUNNING (1<<1)
    #define WAITING (1<<2)
    #define EXITED  (1<<3)
    int16_t exit_code;
    uint32_t prio;
    uint32_t time;
    addr_t user_stack;
    addr_t kern_stack;
    struct list_head list;
} TaskStruct;
#define EXIT_CODE_OK   1
#define EXIT_CODE_KILL 2

extern struct list_head rq, wq, eq;
#define get_rq_count() (rq_len)
#define get_wq_count() (wq_len)
#define get_eq_count() (eq_len)
#define is_rq_empty() (rq.next == &rq)
#define is_wq_empty() (wq.next == &wq)
#define is_eq_empty() (eq.next == &eq)
#define get_task_count() (rq_len + wq_len + eq_len)

int32_t __exec(void *trap_frame, void(*prog)(), char *const argv[]);
int32_t __fork(void *trap_frame);
TaskStruct *get_current();
void try_schedule();
void idle();
void kill_zombies();
void switch_to(TaskStruct *curr, TaskStruct *next);
void main_thread_init();
void thread_release(TaskStruct *curr, int16_t ec);

uint32_t create_kern_task(void(*prog)(), void *arg);
uint32_t create_user_task(void(*prog)());

void thread_trampoline(void(*func)(void *), void *arg);
void __thread_trampoline();
void from_el1_to_el0();
void fork_handler();

#endif /* _LINUX_SCHED_ */