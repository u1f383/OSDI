#include <types.h>
#include <interrupt/svc.h>
#include <lib/printf.h>

uint64_t svc_table[SVC_NUM] =
{
    svc0_handler,
    0,
};

void svc0_handler()
{
    uint64_t spsr_el1 = read_sysreg(spsr_el1);
    uint64_t elr_el1 = read_sysreg(elr_el1);
    uint64_t esr_el1 = read_sysreg(esr_el1);
    
    printf("%x, %x, %x\r\n", spsr_el1, elr_el1, esr_el1);
    while (1);
}