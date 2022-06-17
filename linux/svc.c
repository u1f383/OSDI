#include <initramfs.h>
#include <fs.h>
#include <types.h>
#include <irq.h>
#include <gpio.h>
#include <printf.h>
#include <sched.h>
#include <signal.h>
#include <mm.h>

int svc_getpid();
int svc_mbox_call(unsigned char ch, unsigned int *mbox);
void svc_exit(int16_t status);
int64_t svc_uartread(char buf[], uint64_t size);
int64_t svc_uartwrite(const char buf[], uint64_t size);
void svc_test();

void *svc_table[] =
{
    svc_getpid, // 0
    svc_uartread, // 1
    svc_uartwrite, // 2
    svc_exec, // 3
    svc_fork, // 4
    svc_exit, // 5
    svc_mbox_call, // 6
    svc_kill, // 7
    svc_signal, // 8
    svc_sigkill, // 9
    svc_mmap, // 10
    svc_open, // 11
    svc_close, // 12
    svc_write, // 13
    svc_read, // 14
    svc_mkdir, // 15
    svc_mount, // 16
    svc_chdir, // 17
    svc_lseek64, // 18
    svc_ioctl, // 19
    svc_sync,
    svc_sigreturn, // 21
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
    
    return async_uart_recv_num(buf, size);
}

int64_t svc_uartwrite(const char *buf, uint64_t size)
{
    if (size > 0x1000 || size == 0)
        return -1;
    
    return async_uart_send_num(buf, size);
}

void svc_exit(int16_t status)
{
    thread_release(current, status);
}

int svc_mbox_call(unsigned char ch, unsigned int *mbox)
{
    return mailbox_call_wrapper(ch, mbox);
}
