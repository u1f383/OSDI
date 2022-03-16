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

void uart_init();

void uart_send(char c);
void uart_send_num(char *buf, int num);
char uart_recv_one();
void uart_recv_num(char *buf, int num);

void uart_sendstr(const char *str);
void uart_recvline(char *ptr);

void uart_cmd(char *ptr);

#endif /* _GPIO_UART_H_ */