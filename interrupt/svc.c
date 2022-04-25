#include <types.h>
#include <interrupt/svc.h>
#include <interrupt/irq.h>
#include <gpio/uart.h>
#include <gpio/mailbox.h>
#include <lib/printf.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/mm.h>
#include <init/initramfs.h>

int svc_getpid();
int svc_exec(const char *name, char *const argv[]);
int svc_fork();
int svc_mbox_call(unsigned char ch, unsigned int *mbox);
void svc_exit(int16_t status);
void svc_kill(int pid);
void svc_signal(int SIGNAL, void (*handler)());
void svc_sigkill(int pid, int SIGNAL);
int64_t svc_uartread(char buf[], uint64_t size);
int64_t svc_uartwrite(const char buf[], uint64_t size);
void svc_sigreturn();
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
    svc_signal,
    svc_sigkill,
    svc_sigreturn,
    svc_test,
    0,
};

void svc_sigreturn()
{
    void *trap_frame = read_normreg(x8);
    sigctx_restore(trap_frame);

    disable_intr();
    kfree(current->signal_ctx->tf);
    kfree(current->signal_ctx->user_stack);
    kfree(current->signal_ctx);
    current->signal_ctx = NULL;
    enable_intr();
}

void svc_signal(int SIGNAL, void (*handler)())
{
    Signal *signal = new_signal(SIGNAL, handler);
    
    if (current->signal == NULL)
        current->signal = signal;
    else {
        disable_intr();
        list_add(&signal->list, &current->signal->list);
        enable_intr();
    }
}

void svc_sigkill(int pid, int SIGNAL)
{
    if (current->signal == NULL) {
        default_sighand[SIGNAL - 1](pid);
    } else {
        Signal *iter = current->signal;
        do {
            if (iter->signo == SIGNAL) {
                void *trap_frame = read_normreg(x8);
                sigctx_update(trap_frame, iter->handler);
                break;
            }
            iter = container_of(iter->list.next, Signal, list);
        } while (iter != current->signal);
    }
}

int svc_getpid()
{
    return current->pid;
}

int64_t svc_uartread(char buf[], uint64_t size)
{
    if (size > 0x1800 || size == 0)
        return -1;
    
    async_uart_recv_num(buf, size);
    return size;
}

int64_t svc_uartwrite(const char buf[], uint64_t size)
{
    if (size > 0x1800 || size == 0)
        return -1;
    
    async_uart_send_num(buf, size);
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
    sigkill_handler(pid);
}

void svc_test()
{
    uint64_t spsr_el1 = read_sysreg(spsr_el1);
    uint64_t elr_el1 = read_sysreg(elr_el1);
    uint64_t esr_el1 = read_sysreg(esr_el1);
    
    printf("%x, %x, %x\r\n", spsr_el1, elr_el1, esr_el1);
    while (1);
}