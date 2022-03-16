#include <init/initramfs.h>
#include <kernel/kernel.h>
#include <lib/printf.h>
#include <gpio/uart.h>
#include <arm/mm.h>

void kernel()
{
    /* UART init */
    // uart_init();
    // printf_init(uart_sendstr);
    // char *archive = (char *) CPIO_BASE_ADDR;
    // cpio_ls(archive);

    /* MM init */
    mem_map_init();
    node_init();

    char *string = (char *) buddy_alloc(0x10);

    while (1);
}