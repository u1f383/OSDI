#include <gpio.h>
#include <util.h>
#include <vm.h>
#include <mm.h>
#include <fs.h>
#include <sched.h>
#include <stdarg.h>

#define UART_BUF_SIZE 0x400

AuxRegs *aux_regs;
char uart_rx_rb[ UART_BUF_SIZE ];
char uart_tx_rb[ UART_BUF_SIZE ];

int32_t uart_rx_head = 0,
        uart_rx_tail = 0;
#define IS_RX_EMPTY (uart_rx_head == uart_rx_tail)
#define IS_RX_FILL ((uart_rx_head == uart_rx_tail - 1) || \
                    ((uart_rx_head == 0) && (uart_rx_tail == UART_BUF_SIZE - 1)))

int32_t uart_tx_head = 0,
        uart_tx_tail = 0;
#define IS_TX_EMPTY (uart_tx_head == uart_tx_tail)
#define IS_TX_FILL ((uart_tx_head == uart_tx_tail - 1) || \
                    ((uart_tx_head == 0) && (uart_tx_tail == UART_BUF_SIZE - 1)))

int uart_write(struct file *file, const void *buf, uint64_t len);
int uart_read(struct file *file, void *buf, uint64_t len);
int fb_write(struct file *file, const void *buf, uint64_t len);
int fb_ioctl(struct file *file, unsigned long request, va_list args);

const struct file_operations uart_file_ops = {
    .open = vfs_open,
    .write = uart_write,
    .read = uart_read,
    .close = vfs_close,
    .lseek64 = vfs_lseek64,
    .mknod = vfs_mknod,
    .ioctl = vfs_ioctl,
};

const struct file_operations fb_file_ops = {
    .open = vfs_open,
    .write = fb_write,
    .read = vfs_read,
    .close = vfs_close,
    .lseek64 = vfs_lseek64,
    .mknod = vfs_mknod,
    .ioctl = fb_ioctl,
};

void uart_init()
{
    aux_regs = ((AuxRegs *)AUX_PERIF_REG_MAP_BASE);

    set_bit(aux_regs->enb, AUXENB_Mini_UART_enable_BIT);
    /* Disable tran/recv */
    set_zero(aux_regs->mu_cntl, 0, sizeof(reg32));
    /* Disable tran/recv interrupt */
    set_zero(aux_regs->mu_ier, 0, sizeof(reg32));
    /* Make UART works at 8-bit mode */
    set_value(aux_regs->mu_lcr, 0b11, AUXMULCR_data_size_BIT, AUXMULCR_ALWAYS_BIT);
    /* Disable auto-flow control (?) */
    set_value(aux_regs->mu_mcr, 0, AUXMUMCR_RTS_BIT, AUXMUMCR_RESERVED_BIT);
    /* Setup the baudrate */
    set_value(aux_regs->mu_baud, BAUDRATE_REG, AUXMUBAUD_Baudrate_BIT, AUXMUBAUD_RESERVED_BIT);
    /* Disable tran/recv FIFO */
    set_value(aux_regs->mu_iir, 0b110, AUXMUIIR_Interrupt_pending_BIT, AUXMUIIR_ALWAYS_ZERO_BIT);
    /* Enable tran/recv again */
    set_value(aux_regs->mu_cntl, 0b11, AUXMUCNTL_Receiver_enable_BIT, AUXMUCNTL_Enable_receive_Auto_FC_using_RTS_BIT);

    /* Update GPIO14/15 to alt5 because of minu uart */
    set_value(*GPIO_GPFSEL1_REG, 0b010, GPIO_FSEL14_BIT, GPIO_FSEL14_BIT + GPIO_FSEL_SIZE);
    set_value(*GPIO_GPFSEL1_REG, 0b010, GPIO_FSEL15_BIT, GPIO_FSEL15_BIT + GPIO_FSEL_SIZE);
    
    /* Disable pull-up/down control because pins are always connected */
    set_value(*GPIO_GPPUD_REG, GPIO_GPPUD_Off, 0, sizeof(reg32));
    delay(150);
    /* Send the modification signal to GPIO14/15 */
    set_value(*GPIO_GPPUDCLK0_REG, 0b11, 14, 16);
    delay(150);

    /**
     * Remove control signal by writing gppud, and
     * remove clock by writing GPPUDCLK0
     */
    set_value(*GPIO_GPPUD_REG, GPIO_GPPUD_Off, 0, sizeof(reg32));
    set_value(*GPIO_GPPUDCLK0_REG, GPIO_GPPUDCLK_NO_EFFECT, 0, sizeof(reg32));
}

void uart_intr_handler(reg32 orig_ier)
{
    /* Transmit holding register empty */
    if (aux_regs->mu_iir & 0b010) {
        while (!IS_TX_EMPTY) {
            set_value(aux_regs->mu_io, uart_tx_rb[uart_tx_tail],
                        AUXMUIO_Transmit_data_write_BIT, AUXMUIO_RESERVED_BIT);
            uart_tx_tail = (uart_tx_tail+1) % UART_BUF_SIZE;
        }
        orig_ier &= ~(0b10);
    } else if ((aux_regs->mu_iir & 0b100) && !IS_RX_FILL) {
        /* Receiver holds valid byte */
        uart_rx_rb[uart_rx_head] = \
                get_bits(aux_regs->mu_io, AUXMUIO_Receive_data_read_BIT, AUXMUIO_RESERVED_BIT);
        uart_rx_head = (uart_rx_head+1) % UART_BUF_SIZE;
    }

    /* Unmask UART interrupt */
    set_value(aux_regs->mu_ier, orig_ier, AUXMUIER_Enable_receive_interrupts_BIT, AUXMUIER_RESERVED_BIT);
}

void uart_send(char c)
{
    while (!get_bits(aux_regs->mu_lsr, AUXMULSR_Transmitter_empty_BIT, AUXMULSR_Transmitter_idle_BIT));
    set_value(aux_regs->mu_io, c, AUXMUIO_Transmit_data_write_BIT, AUXMUIO_RESERVED_BIT);
}

void uart_send_num(char *buf, int num)
{
    while (num--)
        uart_send(*buf++);
}

char uart_recv()
{
    while (!get_bits(aux_regs->mu_lsr, AUXMULSR_Data_ready_BIT, AUXMULSR_Receiver_Overrun_BIT));
    return get_bits(aux_regs->mu_io, AUXMUIO_Receive_data_read_BIT, AUXMUIO_RESERVED_BIT);
}

void uart_recv_num(char *buf, int num)
{
    while (num--)
        *buf++ = uart_recv();
}

void uart_cmd(char *ptr)
{
    while ((*ptr = uart_recv()) != '\n' && *ptr != '\r') {
        uart_send(*ptr);
        ptr++;
    }
    *ptr = '\0';
}

void uart_recvline(char *ptr)
{
    while ((*ptr = uart_recv()) != '\n' && *ptr != '\r')
        ptr++;
    *ptr = '\0';
}

void uart_sendstr(const char *str)
{
    while (*str)
        uart_send(*str++);
}

void async_uart_sendstr(const char *str)
{
    if (*str == '\0')
        return;

    while (*str)
    {
        if (IS_TX_FILL) {
            enable_tx_intr();
            continue;
        }

        disable_tx_intr();
        uart_tx_rb[uart_tx_head] = *str++;
        uart_tx_head = (uart_tx_head+1) % UART_BUF_SIZE;
        enable_tx_intr();
    }
}

int async_uart_recv_num(char *buf, int num)
{
    int i = 0;
    while (i < num)
    {
        if (IS_RX_EMPTY) {
            enable_rx_intr();
            continue;
        }
        
        disable_rx_intr();
        *buf = uart_rx_rb[uart_rx_tail];
        uart_rx_tail = (uart_rx_tail+1) % UART_BUF_SIZE;
        i++;
        enable_rx_intr();
    }

    return i;
}

int async_uart_send_num(const char *buf, int num)
{
    int i = 0;
    while (i < num)
    {
        if (IS_TX_FILL) {
            enable_tx_intr();
            continue;
        }

        disable_tx_intr();
        uart_tx_rb[uart_tx_head] = *buf++;
        uart_tx_head = (uart_tx_head+1) % UART_BUF_SIZE;
        i++;
        enable_tx_intr();
    }

    return i;
}

#define MAILBOX0_BASE (PERIF_ADDRESS + 0xb880)
#define MAILBOX1_BASE (PERIF_ADDRESS + 0xb8a0)

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

#define MAILBOX_TAG_REQ_CODE 0
#define MAILBOX_TAG_END 0
#define MAILBOX_CH_PROP 8

#define MBOX_BUF_SIZE 64
volatile unsigned int  __attribute__((aligned(16))) mbox[MBOX_BUF_SIZE];
static unsigned int width, height, pitch, isrgb;
static unsigned int lfb_size = 0;
unsigned char *lfb;

static int mailbox_call(uint32_t channel)
{
    uint64_t addr = virt_to_phys(mbox);
    uint32_t magic = (channel & MAILBOX_CHANNEL_MASK) |
                     ((uint32_t)(uint64_t)addr & MAILBOX_DATA_MASK);
                     
    /* Wait for mailbox not full */
    while (*MAILBOX0_STATUS & MAILBOX_STATUS_FULL);
    /* Pass message to GPU */
    *MAILBOX1_WRITE = magic;
    /* Wait for mailbox not empty */
    while (*MAILBOX0_STATUS & MAILBOX_STATUS_EMPTY);

    return *MAILBOX0_READ == magic;
}

int mailbox_call_wrapper(uint32_t channel, volatile uint32_t *_mbox)
{
    for (int i = 0; i < MBOX_BUF_SIZE; i++)
        mbox[i] = _mbox[i];
    
    return mailbox_call(channel);
}

struct framebuffer_info {
    unsigned int width;
    unsigned int height;
    unsigned int pitch;
    unsigned int isrgb;
};

void mailbox_init(struct vnode *vnode)
{
    mbox[0] = 35 * 4;
    mbox[1] = MAILBOX_REQ_CODE_PROC_REQ;

    mbox[2] = 0x48003; // set phy wh
    mbox[3] = 8;
    mbox[4] = 8;
    mbox[5] = 1024; // FrameBufferInfo.width
    mbox[6] = 768;  // FrameBufferInfo.height

    mbox[7] = 0x48004; // set virt wh
    mbox[8] = 8;
    mbox[9] = 8;
    mbox[10] = 1024; // FrameBufferInfo.virtual_width
    mbox[11] = 768;  // FrameBufferInfo.virtual_height

    mbox[12] = 0x48009; // set virt offset
    mbox[13] = 8;
    mbox[14] = 8;
    mbox[15] = 0; // FrameBufferInfo.x_offset
    mbox[16] = 0; // FrameBufferInfo.y.offset

    mbox[17] = 0x48005; // set depth
    mbox[18] = 4;
    mbox[19] = 4;
    mbox[20] = 32; // FrameBufferInfo.depth

    mbox[21] = 0x48006; // set pixel order
    mbox[22] = 4;
    mbox[23] = 4;
    mbox[24] = 1; // RGB, not BGR preferably

    mbox[25] = 0x40001; // get framebuffer, gets alignment on request
    mbox[26] = 8;
    mbox[27] = 8;
    mbox[28] = 4096; // FrameBufferInfo.pointer
    mbox[29] = 0;    // FrameBufferInfo.size

    mbox[30] = 0x40008; // get pitch
    mbox[31] = 4;
    mbox[32] = 4;
    mbox[33] = 0; // FrameBufferInfo.pitch

    mbox[34] = MAILBOX_TAG_END;

    // this might not return exactly what we asked for, could be
    // the closest supported resolution instead
    if (mailbox_call(MAILBOX_CH_PROP) && mbox[20] == 32 && mbox[28] != 0) {
        width = mbox[5];
        height = mbox[6];
        pitch = mbox[33];
        isrgb = mbox[24];

        lfb_size = mbox[29];
        mbox[28] &= 0x3FFFFFFF;
        lfb = (void *)phys_to_virt(mbox[28]);
    };
}

int uart_write(struct file *file, const void *buf, uint64_t len)
{
    return async_uart_send_num(buf, len);
}

int uart_read(struct file *file, void *buf, uint64_t len)
{
    return async_uart_recv_num(buf, len);
}

int fb_write(struct file *file, const void *buf, uint64_t len)
{
    const char *ptr = buf;
    uint64_t i;

    for (i = 0; i < len && file->f_pos < lfb_size; i++, file->f_pos++)
        *(lfb + file->f_pos) = *ptr++;

    return i;
}

int fb_ioctl(struct file *file, unsigned long request, va_list args)
{
    if (request == 0) {
        struct framebuffer_info *fbinfo = va_arg(args, struct framebuffer_info *);

        fbinfo->width = width;
        fbinfo->height = height;
        fbinfo->pitch = pitch;
        fbinfo->isrgb = isrgb;
    }

    return 0;
}
