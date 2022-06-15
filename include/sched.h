#ifndef _SCHED_H_
#define _SCHED_H_

#include <types.h>
#include <list.h>
#include <irq.h>
#include <signal.h>
#include <initramfs.h>
#include <fs.h>
#include <vm.h>

#define THREAD_STACK_SIZE 0x4000
#define USER_THREAD_BASE_ADDR 0xffffffffb000
#define current get_current()

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
    void *user_stack;
    void *kern_stack;
    struct list_head list;
    Signal *signal;
    SignalCtx *signal_ctx;
    uint8_t signal_queue;
    mm_struct *mm;
    struct fdt_struct *fdt;
    struct vnode *workdir;
} TaskStruct;

typedef struct _TaskQueue {
    struct list_head list;
    uint32_t len;
    uint8_t lock;
} TaskQueue;

#define EXIT_CODE_OK   1
#define EXIT_CODE_KILL 2

extern TaskQueue rq, eq;
#define IS_RQ_EMPTY (rq.len == 0)
#define IS_EQ_EMPTY (eq.len == 0)

int32_t svc_exec(const char *name, char *const argv[]);
int32_t svc_fork();

TaskStruct *get_current();
void task_queue_init();
void try_schedule();
void schedule();
void idle();
void kill_zombies();
void switch_to(TaskStruct *curr, TaskStruct *next, pgd_t next_pgd);
void main_thread_init();
void thread_release(TaskStruct *curr, int16_t ec);
void call_sigreturn();

uint32_t create_kern_task(void(*prog)(), void *arg);
uint32_t create_user_task(const char *pathname);

void thread_trampoline(void(*func)(void *), void *arg);
void __thread_trampoline();
void from_el1_to_el0();
void fork_trampoline();

extern TaskStruct *main_task;

#endif /* _SCHED_H_ */