#include <types.h>
#include <interrupt/svc.h>
#include <gpio/uart.h>
#include <lib/printf.h>

void getpid();
int64_t uartread(char buf[], uint64_t size);
int64_t uartwrite(const char buf[], uint64_t size);
int exec(const char *name, char *const argv[]);
int fork();
void exit(int status);
int mbox_call(unsigned char ch, unsigned int *mbox);
void kill(int pid);

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

void getpid()
{

}

int64_t uartread(char buf[], uint64_t size)
{
    if (size > 0x100)
        return -1;
    
    uart_recv_num(buf, size);
    return size;
}

int64_t uartwrite(const char buf[], uint64_t size)
{
    if (size > 0x100)
        return -1;
    
    uart_send_num(buf, size);
    return size;
}

int exec(const char *name, char *const argv[])
{
    
}

int fork()
{

}

void exit(int status)
{

}

int mbox_call(unsigned char ch, unsigned int *mbox)
{

}

void kill(int pid)
{
    
}

void svc0_handler()
{
    uint64_t spsr_el1 = read_sysreg(spsr_el1);
    uint64_t elr_el1 = read_sysreg(elr_el1);
    uint64_t esr_el1 = read_sysreg(esr_el1);
    
    printf("%x, %x, %x\r\n", spsr_el1, elr_el1, esr_el1);
    while (1);
}