#include <init/initramfs.h>
#include <kernel/kernel.h>
#include <kernel/dtb.h>
#include <lib/printf.h>
#include <gpio/uart.h>
#include <arm/mm.h>

extern uint64_t dtb_base;
extern void from_el1_to_el0(uint64_t prog_ep);
extern void timer_init(uint64_t secs);

void init_cpio(int node_type, char *prop_name, char *value, int len)
{
    if (!strcmp(prop_name, "linux,initrd-start"))
    {
        uint32_t prop_val_32 = (*(uint32_t *) value);
        cpio_start = (char *) ((uint64_t) endian_xchg_32(prop_val_32));
    }
    if (!strcmp(prop_name, "linux,initrd-end"))
    {
        uint32_t prop_val_32 = (*(uint32_t *) value);
        cpio_end = (char *) ((uint64_t) endian_xchg_32(prop_val_32));
    }
}

void usage()
{
    uart_sendstr(""
        "**************** [ HELP ] ****************\r\n"
        "help     \t\t: print this help menu\r\n"
        "ls       \t\t: list files in the initramfs\r\n"
        "run_user \t\t: run user program\r\n"
    );
}

void kernel()
{
    uint64_t boot_time = read_sysreg(cntpct_el0);

    /* UART init */
    uart_init();
    printf_init(uart_sendstr);
    
    /* MM init */
    mem_map_init();
    node_init();

    /* CPIO init */
    parse_dtb((char *) dtb_base, init_cpio);

    /* Timer init */
    printf("boot_time: %x\r\n", boot_time);
    // timer_init(2);

    /* Enable UART interrupt */
    uart_eint();
    async_uart_sendstr("hello world !");

    char cmd[0x20];
    while (1)
    {
        // async_uart_sendstr("# ");
        // uart_cmd(cmd);
        // uart_sendstr("\r\n");

        // if (!strcmp(cmd, "help")) {
        //     usage();
        // } else if (!strcmp(cmd, "ls")) {
        //     cpio_ls();
        // } else if (!strcmp(cmd, "run_user")) {
        //     // uart_sendstr("filename: ");
        //     // uart_cmd(cmd);
        //     // uart_sendstr("\r\n");

        //     char *program = cpio_find_file("rootfs//user_program");
        //     if (program == NULL)
        //         printf("File not found.\r\n");
        //     else
        //         from_el1_to_el0((uint64_t) program);
        // }
    }
}