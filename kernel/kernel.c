#include <initramfs.h>
#include <tmpfs.h>
#include <irq.h>
#include <dtb.h>
#include <printf.h>
#include <gpio.h>
#include <mm.h>
#include <vm.h>
#include <sched.h>
#include <util.h>
#include <sdhost.h>
#include <fat32.h>

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

void foo()
{
    for (int i = 0; i < 10; i++) {
        printf("Thread id: 0x%lx %d\r\n", current, i);
        delay(1000000);
        schedule();
    }
}

int cpio_init_cb(char *node_name, char *prop_name, char *value, int len)
{
    if (prop_name) {
        if (!strcmp(prop_name, "linux,initrd-start"))
            cpio_start = endian_xchg_32(*(uint32_t *)value) + MM_VIRT_KERN_START;
        else if (!strcmp(prop_name, "linux,initrd-end"))
            cpio_end = endian_xchg_32(*(uint32_t *)value) + MM_VIRT_KERN_START;
        else if (!strcmp(prop_name, "memreserve"))
            phys_mem_end = endian_xchg_32(*(uint32_t *)value);
    }

    if (cpio_start && cpio_end) {
        register_mem_reserve(cpio_start, cpio_end);
        return 1;
    }

    return 0;
}

static inline void counter_timer_init()
{
    /* Counter-timer Kernel Control register */
    uint64_t tmp;
    asm volatile("mrs %0, cntkctl_el1" : "=r"(tmp));
    tmp |= 1;
    asm volatile("msr cntkctl_el1, %0" : : "r"(tmp));
}

void kernel(void *dtb_base)
{
    uint64_t boot_time = read_sysreg(cntpct_el0);

    sd_init();
    uart_init();
    printf_init(uart_sendstr);
    dtb_init(dtb_base);
    parse_dtb(cpio_init_cb);
    page_init();
    buddy_init();
    slab_init();
    uart_enable_intr();
    counter_timer_init();
    task_queue_init();
    main_thread_init();
    register_filesystem(&tmpfs);
    
    printf("boot_time: %x\r\n", boot_time);

    // for (int i = 0; i < 3; i++)
    //     create_kern_task(foo, NULL);

    if (create_user_task("/initramfs/vfs2.img") != 0) {
        printf("file not found!\r\n");
        hangon();
    }

    idle();
}
