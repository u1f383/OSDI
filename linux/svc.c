#include <types.h>
#include <irq.h>
#include <gpio.h>
#include <printf.h>
#include <sched.h>
#include <signal.h>
#include <mm.h>
#include <initramfs.h>

int svc_getpid();
int svc_mbox_call(unsigned char ch, unsigned int *mbox);
void svc_exit(int16_t status);
int64_t svc_uartread(char buf[], uint64_t size);
int64_t svc_uartwrite(const char buf[], uint64_t size);
void svc_test();

void *svc_table[] =
{
    svc_getpid,
    svc_uartread,
    svc_uartwrite,
    svc_exec,
    svc_fork,
    svc_exit,
    svc_mbox_call,
    svc_kill,
    svc_signal,
    svc_sigkill,
    svc_sigreturn,
};

#define SVC_NUM (sizeof(svc_table) / sizeof(svc_table[0]))
int32_t sysno_check(uint32_t sysno)
{
    return sysno >= SVC_NUM ? -1 : 0;
}

int svc_getpid()
{
    return current->pid;
}

int64_t svc_uartread(char buf[], uint64_t size)
{
    if (size > 0x1000 || size == 0)
        return -1;
    
    if (async_uart_recv_num(buf, size) == 0)
        return 0;
        
    return size;
}

int64_t svc_uartwrite(const char *buf, uint64_t size)
{
    if (size > 0x1000 || size == 0)
        return -1;
    
    if (async_uart_send_num(buf, size) == 0)
        return 0;

    return size;
}

void svc_exit(int16_t status)
{
    thread_release(current, status);
}

int svc_mbox_call(unsigned char ch, unsigned int *mbox)
{
    return mailbox_call(ch, mbox);
}
