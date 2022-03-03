#include "util.h"
#include "gpio/uart.h"
#include "gpio/base.h"
#include "lib/printf.h"

#define PM_REG (PERIF_ADDRESS + 0x100000)
#define PM_PASSWORD 0x5A000000

#define PM_RSTC ((reg32 *)(PM_REG + 0x1C))
#define PM_RSTC_WRCFG_FULL_RESET 0x20

#define PM_WDOG ((reg32 *)(PM_REG + 0x24))
void reset(int tick);
void cancel_reset();

/* Reboot pi after watchdog timer expire */
void reset(int tick)
{
    set_value(*PM_RSTC, PM_PASSWORD | PM_RSTC_WRCFG_FULL_RESET, 0, sizeof(reg32)); // Full reset
    set_value(*PM_WDOG, PM_PASSWORD | tick, 0, sizeof(reg32)); // Number of watchdog tick
}

void cancel_reset()
{
    set_value(*PM_RSTC, PM_PASSWORD | 0, 0, sizeof(reg32)); /* Disable rstc */
    set_value(*PM_WDOG, PM_PASSWORD | 0, 0, sizeof(reg32)); /* Disable wdog */
}

#define MAILBOX0_BASE (PERIF_ADDRESS + 0xB880)
#define MAILBOX1_BASE (PERIF_ADDRESS + 0xB8A0)

#define MAILBOX0_READ ((reg32 *)(MAILBOX0_BASE + 0x0))
#define MAILBOX0_STATUS ((reg32 *)(MAILBOX0_BASE + 0x18))
#define MAILBOX1_WRITE ((reg32 *)(MAILBOX0_BASE + 0x20))
#define MAILBOX_CHANNEL_MASK 0x0000000f
#define MAILBOX_DATA_MASK 0xfffffff0

#define MAILBOX_STATUS_FULL 0x80000000
#define MAILBOX_STATUS_EMPTY 0x40000000

#define MAILBOX_REQ_CODE_PROC_REQ 0x00000000
#define MAILBOX_RES_CODE_REQ_SUCC 0x80000000
#define MAILBOX_RES_CODE_REQ_ERR 0x80000001

#define MAILBOX_TAG_GET_BOARD_REVISION 0x00010002
#define MAILBOX_TAG_GET_ARM_MEMORY 0x00010005
#define MAILBOX_TAG_REQ_CODE 0
#define MAILBOX_TAG_END 0

#define MAILBOX0_CHANNEL_PROPERTYTAGS_ARMVC 0x8

volatile uint32_t __attribute__((aligned(0b10000))) mbox_buffer[36];
int mailbox_call(uint32_t channel, volatile uint32_t *mbox);
void get_board_revision(uint32_t *frev);
void get_arm_memory(uint32_t *base, uint32_t *size);

int mailbox_call(uint32_t channel, volatile uint32_t *mbox)
{
    uint32_t magic = (channel & MAILBOX_CHANNEL_MASK) |
                    ( (uint32_t) (((uint64_t) mbox) & MAILBOX_DATA_MASK) );
    /* Wait for mailbox not full */
    while (get_bits(*MAILBOX0_STATUS, 0, 32) & MAILBOX_STATUS_FULL);
    /* Pass message to GPU */
    *MAILBOX1_WRITE = magic;
    /* Get the GPU response */
    
    /* Wait for mailbox not empty */
    while (get_bits(*MAILBOX0_STATUS, 0, 32) & MAILBOX_STATUS_EMPTY);
    return *MAILBOX0_READ == magic;
}

void get_board_revision(uint32_t *frev)
{
    mbox_buffer[0] = 7 * 4;
    mbox_buffer[1] = MAILBOX_REQ_CODE_PROC_REQ;
    /* Tags */
    mbox_buffer[2] = MAILBOX_TAG_GET_BOARD_REVISION;
    mbox_buffer[3] = sizeof(uint32_t); /* Max value buffer size */
    mbox_buffer[4] = MAILBOX_TAG_REQ_CODE;
    mbox_buffer[5] = 0; /* Value buffer */
    mbox_buffer[6] = MAILBOX_TAG_END;

    int ret = mailbox_call(MAILBOX0_CHANNEL_PROPERTYTAGS_ARMVC, mbox_buffer);
    if (mbox_buffer[1] != MAILBOX_RES_CODE_REQ_SUCC || !ret)
        { /* TODO: Error handle */ }
    *frev = mbox_buffer[5];
}

void get_arm_memory(uint32_t *base, uint32_t *size)
{
    mbox_buffer[0] = 8 * 4;
    mbox_buffer[1] = MAILBOX_REQ_CODE_PROC_REQ;
    /* Tags */
    mbox_buffer[2] = MAILBOX_TAG_GET_ARM_MEMORY;
    mbox_buffer[3] = sizeof(uint32_t); /* Max value buffer size */
    mbox_buffer[4] = MAILBOX_TAG_REQ_CODE;
    mbox_buffer[5] = 0; /* Value buffer [0] */
    mbox_buffer[6] = 0; /* Value buffer [1] */
    mbox_buffer[7] = MAILBOX_TAG_END;

    int ret = mailbox_call(MAILBOX0_CHANNEL_PROPERTYTAGS_ARMVC, mbox_buffer);
    if (mbox_buffer[1] != MAILBOX_RES_CODE_REQ_SUCC || !ret)
        { /* TODO: Error handle */ }
    *base = mbox_buffer[5];
    *size = mbox_buffer[6];
}

void usage()
{
    uart_sendstr(""
        "help\t\t: print this help menu\r\n"
        "hello\t\t: print Hello World!\r\n"
        "reboot\t\t: reboot the device\r\n"
        "mailbox\t\t: show board revision and arm memory information\r\n"
    );
}

void kernel()
{
    uart_init();

    char cmd[0x10];
    char buf[0x10];

    uint32_t base, size, frev;
    
    get_arm_memory(&base, &size);
    get_board_revision(&frev);

    while (1)
    {
        uart_sendstr("# ");
        uart_recvline(cmd);
        uart_sendstr("\r\n");

        if (!strcmp(cmd, "hello")) {
            uart_sendstr("Hello World!\n\r");
        } else if (!strcmp(cmd, "help")) {
            usage();
        } else if (!strcmp(cmd, "reboot")) {
            reset(10);
            while (1);
        } else if (!strcmp(cmd, "mailbox")) {
            uart_sendstr("@Arm base memory: ");
            lutoa(buf, base, 16);
            uart_sendstr(buf);
            uart_sendstr("\r\n");
            
            uart_sendstr("@Arm base size: ");
            lutoa(buf, size, 16);
            uart_sendstr(buf);
            uart_sendstr("\r\n");
            
            uart_sendstr("@Board revision: ");
            lutoa(buf, frev, 16);
            uart_sendstr(buf);
            uart_sendstr("\r\n");
        }
    }
}