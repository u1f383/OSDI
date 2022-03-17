#include <init/initramfs.h>
#include <kernel/kernel.h>
#include <kernel/dtb.h>
#include <lib/printf.h>
#include <gpio/uart.h>
#include <arm/mm.h>

extern uint64_t dtb_base;

void callback_test()
{
    printf("[HERE IS CALLBACK] cpio_start: 0x%lx\r\n", cpio_start);
}

void kernel()
{
    /* UART init */
    uart_init();
    printf_init(uart_sendstr);

    /* Device tree */
    parse_dtb((char *) dtb_base, callback_test);

    /* MM init */
    mem_map_init();
    node_init();
    char *str = (char *) buddy_alloc(0x10);
    strcpy(str, "Hello, world !\r\n");
    printf(str);

    char *archive = (char *) cpio_start;
    cpio_ls(archive);

    while (1);
}