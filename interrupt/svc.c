#include <types.h>
#include <interrupt/svc.h>
#include <interrupt/irq.h>
#include <gpio/uart.h>
#include <gpio/mailbox.h>
#include <lib/printf.h>
#include <linux/sched.h>
#include <init/initramfs.h>

int svc_getpid();
int svc_exec(const char *name, char *const argv[]);
int svc_fork();
int svc_mbox_call(unsigned char ch, unsigned int *mbox);
void svc_exit(int16_t status);
void svc_kill(int pid);
int64_t svc_uartread(char buf[], uint64_t size);
int64_t svc_uartwrite(const char buf[], uint64_t size);
void svc_test();

uint64_t svc_table[SVC_NUM] =
{
    svc_getpid,
    svc_uartread,
    svc_uartwrite,
    svc_exec,
    svc_fork,
    svc_exit,
    svc_mbox_call,
    svc_kill,
    svc_test,
    0,
};

int svc_getpid()
{
    return current->pid;
}

int64_t svc_uartread(char buf[], uint64_t size)
{
    if (size > 0x1800 || size == 0)
        return -1;
    
    uart_recv_num(buf, size);
    return size;
}

int64_t svc_uartwrite(const char buf[], uint64_t size)
{
    if (size > 0x1800 || size == 0)
        return -1;
    
    uart_send_num(buf, size);
    return size;
}

int svc_exec(const char *name, char *const argv[])
{
    void *trap_frame = read_normreg(x8);
    char *prog = cpio_find_file(name);
    if (prog == NULL)
        return -1;

    return __exec(trap_frame, prog, argv);
}

int svc_fork()
{
    void *trap_frame = read_normreg(x8);
    return __fork(trap_frame);
}

void svc_exit(int16_t status)
{
    thread_release(current, status);
}

int svc_mbox_call(unsigned char ch, unsigned int *mbox)
{
    return mailbox_call(ch, mbox);
}

void svc_kill(int pid)
{
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
}

void svc_test()
{
    uint64_t spsr_el1 = read_sysreg(spsr_el1);
    uint64_t elr_el1 = read_sysreg(elr_el1);
    uint64_t esr_el1 = read_sysreg(esr_el1);
    
    printf("%x, %x, %x\r\n", spsr_el1, elr_el1, esr_el1);
    while (1);
}