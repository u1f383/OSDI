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

    /* Mem init */
    page_init();
    buddy_init();

    /* UART init */
    uart_init();
    printf_init(uart_sendstr);
    
    /* CPIO init */
    parse_dtb((char *) dtb_base, init_cpio);

    /* Timer init */
    printf("boot_time: %x\r\n", boot_time);

    /* Enable UART interrupt */
    uart_eint();
    
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
            // char *ptr = cmd;
            // char *tmp, *msg;
            // uint32_t timeout;

            // while (*ptr && *ptr++ != ' ');
            // if (*ptr == '\0')
            //     continue;
            // tmp = ptr;

            // while (*ptr && *++ptr != ' ')
            // if (*ptr == '\0')
            //     continue;
            // *ptr++ = '\0';
            // timeout = strtou(ptr, 10);

            // msg = buddy_alloc(10);
            // strcpy(msg, tmp);
            // add_timer(uart_sendstr, msg, timeout);
        }
    }
}
