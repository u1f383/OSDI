#include <util.h>
#include <types.h>
#include <txrx/txrx.h>
#include <gpio/uart.h>
#include <gpio/base.h>
#include <lib/printf.h>

#define KERNEL_BASE_ADDR 0x80000
#define PM_REG (PERIF_ADDRESS + 0x100000)
#define PM_PASSWORD 0x5A000000
#define PM_RSTC ((reg32 *)(PM_REG + 0x1C))
#define PM_RSTC_WRCFG_FULL_RESET 0x20
#define PM_WDOG ((reg32 *)(PM_REG + 0x24))

void reset(int tick);
void cancel_reset();

static char *kernel_addr =(char *) KERNEL_BASE_ADDR;
extern uint64_t dtb_base;

/* Reboot pi after watchdog timer expire */
void reset(int tick)
{
    set_value(*PM_RSTC, PM_PASSWORD | PM_RSTC_WRCFG_FULL_RESET, 0, sizeof(reg32)); // Full reset
    set_value(*PM_WDOG, PM_PASSWORD | tick, 0, sizeof(reg32)); // Number of watchdog tick
    while (1); /* Waiting for countdown */
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
    mbox_buffer[3] = 8; /* Max value buffer size */
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
        "**************** [ HELP ] ****************\r\n"
        "help     \t\t: print this help menu\r\n"
        "reboot   \t\t: reboot the device\r\n"
        "mailbox  \t\t: show board revision and arm memory information\r\n"
        "load_kern\t\t: transmit kernel image to 0x80000\r\n"
        "run_kern \t\t: run real kernel\r\n"
    );
}

void load_kernel()
{
    static int _kern_is_loaded = 0;
    char buf[ 1024 + sizeof(Packet) ];
    char *_kern_addr = kernel_addr;
    Packet *packet = (Packet *) buf;
    uint8_t checksum;
    
    if (_kern_is_loaded)
    {
        uart_sendstr("kernel has been loaded !\r\n");
        return;
    }

    buf[4] = '\0';
    uart_sendstr(MAGIC_1_STR);
    uart_recv_num(buf, 4);

    if (memcmp(buf, MAGIC_2_STR, 4)) {
        uart_sendstr(FAILED_STR);
        return;
    }
    uart_sendstr(MAGIC_3_STR);
    
    while (1)
    {
        uart_recv_num((char *) packet, sizeof(Packet));
        if (packet->status == STATUS_END)
            break;

        uart_recv_num(packet->data, packet->size);
        memcpy(_kern_addr, packet->data, packet->size);
        _kern_addr += packet->size;

        checksum = calc_checksum((unsigned char*) packet->data, packet->size);

        if (checksum != packet->checksum)
        {
            packet->status = STATUS_BAD;
            packet->checksum = checksum;
        }
        else
            packet->status = STATUS_OK;

        uart_send_num((char *) packet, sizeof(Packet));
    }
    packet->status = STATUS_FIN;
    uart_send_num((char *) packet, sizeof(Packet));
    _kern_is_loaded = 1;
}

__attribute__((noreturn))
void run_kernel()
{
    ( *(void(*)(uint64_t) ) kernel_addr )(dtb_base);
    while (1);
}

void loader()
{
    uart_init();
    printf_init(uart_sendstr);

    char cmd[0x10];
    char buf[0x10];

    uint32_t base, size, frev;
    
    get_arm_memory(&base, &size);
    get_board_revision(&frev);

    while (1)
    {
        uart_sendstr("# ");
        uart_cmd(cmd);
        uart_sendstr("\r\n");

        if (!strcmp(cmd, "help")) {
            usage();
        } else if (!strcmp(cmd, "reboot")) {
            reset(10);
        } else if (!strcmp(cmd, "mailbox")) {
            uart_sendstr("@Arm base memory: ");
            lutostr(buf, base, 16);
            uart_sendstr(buf);
            uart_sendstr("\r\n");
            
            uart_sendstr("@Arm base size: ");
            lutostr(buf, size, 16);
            uart_sendstr(buf);
            uart_sendstr("\r\n");
            
            uart_sendstr("@Board revision: ");
            lutostr(buf, frev, 16);
            uart_sendstr(buf);
            uart_sendstr("\r\n");
        } else if (!strcmp(cmd, "load_kern")) {
            load_kernel();
        } else if (!strcmp(cmd, "run_kern")) {
            run_kernel();
        }
    }
}