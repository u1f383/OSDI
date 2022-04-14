#include <init/initramfs.h>
#include <interrupt/irq.h>
#include <init/dtb.h>
#include <lib/printf.h>
#include <gpio/uart.h>
#include <arm/mm.h>

extern addr_t dtb_base, dtb_end;
extern void from_el1_to_el0(uint64_t prog_ep);

void init_cpio(char *node_name, char *prop_name, char *value, int len)
{
    if (prop_name) {
        if (!strcmp(prop_name, "linux,initrd-start"))
        {
            cpio_start = endian_xchg_32( *(uint32_t *) value );
        }
        else if (!strcmp(prop_name, "linux,initrd-end"))
        {
            cpio_end = endian_xchg_32( *(uint32_t *) value );
        }
        else if (!strcmp(prop_name, "memreserve"))
        {
            phys_mem_end = endian_xchg_32( *(uint32_t *) value );
        }
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

    /* UART & CPIO init */
    uart_init();
    printf_init(uart_sendstr);
    parse_dtb(init_cpio);
    
    /* Mem init */
    page_init();
    memory_reserve(cpio_start, cpio_end);
    memory_reserve(dtb_base, dtb_end);
    buddy_init();
    slab_init();

    /* Enable UART interrupt */
    uart_eint();

    char cmd[0x20];
    
    printf("boot_time: %x\r\n", boot_time);
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

            msg = kmalloc(10);
            strcpy(msg, tmp);
            add_timer(uart_sendstr, msg, timeout);
            kfree(msg);
        } else if (!strcmp(cmd, "test_mm")) {
            char *ptr[100];
            for (int i = 0; i < 100; i++)
                ptr[i] = kmalloc(i * 4*KB);
            for (int i = 0; i < 100; i++)
                kfree(ptr[i]);

            char *slab_ptr[100];
            for (int i = 0; i < 100; i++)
                slab_ptr[i] = kmalloc(i * 0x10);
            for (int i = 0; i < 100; i++)
                kfree(slab_ptr[i]);
        }
    }
}
