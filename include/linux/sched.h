#ifndef _LINUX_SCHED_
#define _LINUX_SCHED_

#include <types.h>
#include <list.h>
#define TASK_NUM 8
#define THREAD_STACK_SIZE 0x1000
#define TIME_SLOT 0x100000
#define current get_current()

typedef struct _ThreadInfo {
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
    uint64_t fp;
    uint64_t lr;
    uint64_t sp;
    uint64_t pc; /* elr_el1 */
    uint64_t spsr_el1;
} ThreadInfo;

typedef struct _TaskStruct {
    ThreadInfo thread_info;
    int32_t pid;

    uint32_t status;
    #define EMPTY   (0)
    #define STOPPED (1<<0)
    #define RUNNING (1<<1)
    #define WAITING (1<<2)
    #define EXITED  (1<<3)
    uint32_t prio;
    uint32_t time;
    struct list_head list;
} TaskStruct;

extern TaskStruct tasks[ TASK_NUM ];

TaskStruct *get_current();
void context_switch(TaskStruct **curr);
void idle();
void kill_zombies();
void schedule();
void main_thread_init();

uint32_t create_kern_task(void(*prog)(), void *arg);
uint32_t create_user_task(void(*prog)());

#endif /* _LINUX_SCHED_ */