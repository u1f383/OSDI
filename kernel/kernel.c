#include <init/initramfs.h>
#include <interrupt/irq.h>
#include <init/dtb.h>
#include <lib/printf.h>
#include <gpio/uart.h>
#include <arm/mm.h>

extern uint64_t dtb_base;
extern void from_el1_to_el0(uint64_t prog_ep);

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
        "help        : print this help menu\r\n"
        "ls          : list files in the initramfs\r\n"
        "run_user    : run user program\r\n"
        "set_timeout : print msg after secs\r\n"
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

    /* Enable UART interrupt */
    uart_eint();

    add_timer(uart_sendstr, "0000000000000000\r\n", 3);
    add_timer(uart_sendstr, "1111111111111111\r\n", 3);
    add_timer(uart_sendstr, "2222222222222222\r\n", 3);
    add_timer(uart_sendstr, "3333333333333333\r\n", 3);
    add_timer(uart_sendstr, "4444444444444444\r\n", 3);
    add_timer(uart_sendstr, "5555555555555555\r\n", 3);
    add_timer(uart_sendstr, "6666666666666666\r\n", 3);
    add_timer(uart_sendstr, "7777777777777777\r\n", 3);
    add_timer(uart_sendstr, "8888888888888888\r\n", 3);
    add_timer(uart_sendstr, "9999999999999999\r\n", 3);
    add_timer(uart_sendstr, "10101010101010101010101010101010\r\n", 3);
    add_timer(uart_sendstr, "11111111111111111111111111111111\r\n", 3);
    add_timer(uart_sendstr, "12121212121212121212121212121212\r\n", 3);
    add_timer(uart_sendstr, "13131313131313131313131313131313\r\n", 3);
    add_timer(uart_sendstr, "14141414141414141414141414141414\r\n", 3);
    add_timer(uart_sendstr, "15151515151515151515151515151515\r\n", 3);
    add_timer(uart_sendstr, "16161616161616161616161616161616\r\n", 3);
    add_timer(uart_sendstr, "17171717171717171717171717171717\r\n", 3);
    add_timer(uart_sendstr, "18181818181818181818181818181818\r\n", 3);
    add_timer(uart_sendstr, "19191919191919191919191919191919\r\n", 3);

    char cmd[0x20];
    while (1)
    {
        async_uart_sendstr("# ");
        async_uart_cmd(cmd);
        async_uart_sendstr("\r\n");

        if (!strcmp(cmd, "help")) {
            usage();
        } else if (!strcmp(cmd, "ls")) {
            cpio_ls();
        } else if (!strcmp(cmd, "run_user")) {
            char *program = cpio_find_file("rootfs//user_program");
            if (program == NULL)
                printf("File not found.\r\n");
            else
                from_el1_to_el0((uint64_t) program);
        } else if (!strncmp(cmd, "set_timeout", 11)) {
            char *ptr = cmd;
            char *tmp, *msg;
            uint32_t timeout;

            while (*ptr && *ptr++ != ' ');
            if (*ptr == '\0')
                continue;
            tmp = ptr;

            while (*ptr && *++ptr != ' ')
            if (*ptr == '\0')
                continue;
            *ptr++ = '\0';
            timeout = strtou(ptr, 10);

            msg = buddy_alloc(10);
            strcpy(msg, tmp);
            add_timer(uart_sendstr, msg, timeout);
        }
    }
}
