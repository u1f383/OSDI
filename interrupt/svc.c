#include <types.h>
#include <interrupt/svc.h>
#include <gpio/uart.h>
#include <gpio/mailbox.h>
#include <lib/printf.h>
#include <linux/sched.h>
#include <init/initramfs.h>
#include <interrupt/irq.h>

int getpid();
int exec(const char *name, char *const argv[]);
int fork();
int mbox_call(unsigned char ch, unsigned int *mbox);
void exit(int16_t status);
void kill(int pid);
int64_t uartread(char buf[], uint64_t size);
int64_t uartwrite(const char buf[], uint64_t size);

uint64_t svc_table[SVC_NUM] =
{
    getpid,
    uartread,
    uartwrite,
    exec,
    fork,
    exit,
    mbox_call,
    kill,
    svc0_handler,
    0,
};

int getpid()
{
    return current->pid;
}

int64_t uartread(char buf[], uint64_t size)
{
    if (size > 0x1800 || size == 0)
        return -1;
    
    uart_recv_num(buf, size);
    return size;
}

int64_t uartwrite(const char buf[], uint64_t size)
{
    if (size > 0x1800 || size == 0)
        return -1;
    
    uart_send_num(buf, size);
    return size;
}

int exec(const char *name, char *const argv[])
{
    char *program = cpio_find_file(name);
    if (program == NULL)
        return -1;

    
}

int fork()
{
    return __fork();
}

void exit(int16_t status)
{
    thread_release(current, status);
}

int mbox_call(unsigned char ch, unsigned int *mbox)
{
    // disable_timer();
    return mailbox_call(ch, mbox);
}

void kill(int pid)
{
    TaskStruct *curr = rq;
    TaskStruct *next = container_of(curr->list.next, TaskStruct, list);
    
    while (next != rq) {
        if (curr->pid == pid) {
            thread_release(curr, EXIT_CODE_KILL);
            break;
        }

        next = container_of(curr->list.next, TaskStruct, list);
        curr = next;
    }
}

void svc0_handler()
{
    uint64_t spsr_el1 = read_sysreg(spsr_el1);
    uint64_t elr_el1 = read_sysreg(elr_el1);
    uint64_t esr_el1 = read_sysreg(esr_el1);
    
    printf("%x, %x, %x\r\n", spsr_el1, elr_el1, esr_el1);
    while (1);
}