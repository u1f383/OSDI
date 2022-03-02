#include "gpio/uart.h"

void kernel()
{
    uart_init();

    while (1) 
    {
        uart_sendstr("> ");

    }
}