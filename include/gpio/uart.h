#ifndef _GPIO_UART_H_
#define _GPIO_UART_H_

#include <util.h>
#include <gpio/base.h>

/**
 * When mini UART is enabled, core clock is fixed to 250 MHz
 */
#define VPU_SYSTEM_CLOCK_FREQ (250 * 1000 * 1000)
#define BAUDRATE 115200
#define BAUDRATE_REG (VPU_SYSTEM_CLOCK_FREQ / (8 * BAUDRATE)) - 1 // 271

#define ARM_INT_REG_BASE (PERIF_ADDRESS + 0xB000)
#define ARM_INT_IRQs1_REG (ARM_INT_REG_BASE + 0x210)

extern AuxRegs *aux_regs;

void uart_init();
void uart_eint();
void uart_intr_handler(reg32 orig_ier);

void uart_send(char c);
void uart_send_num(char *buf, int num);
char uart_recv();
void uart_recv_num(char *buf, int num);

void uart_sendstr(const char *str);
void uart_recvline(char *ptr);

void uart_cmd(char *ptr);

void async_uart_cmd(char *ptr);
void async_uart_sendchr(char c);
void async_uart_sendstr(const char *str);

static inline void disable_uart()
{
    set_value(aux_regs->mu_ier, 0, AUXMUIER_Enable_receive_interrupts_BIT, AUXMUIER_RESERVED_BIT);
}


#endif /* _GPIO_UART_H_ */