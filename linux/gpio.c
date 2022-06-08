#include <gpio.h>
#include <util.h>
#include <vm.h>
#include <mm.h>
#include <sched.h>

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
        if (!IS_TX_EMPTY) {
            set_value(aux_regs->mu_io, uart_tx_rb[uart_tx_tail],
                        AUXMUIO_Transmit_data_write_BIT, AUXMUIO_RESERVED_BIT);
            uart_tx_tail = (uart_tx_tail+1) % UART_BUF_SIZE;
        }
        else {
            orig_ier &= ~(0b10);
        }
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
        if (IS_TX_FILL)
            continue;

        uart_tx_rb[uart_tx_head] = *str++;
        uart_tx_head = (uart_tx_head+1) % UART_BUF_SIZE;
        enable_tx_intr();
    }
}

int async_uart_recv_num(char *buf, int num)
{
    while (num)
    {
        if (IS_RX_EMPTY) {
            enable_rx_intr();
            continue;
        }
        
        *buf = uart_rx_rb[uart_rx_tail];
        uart_rx_tail = (uart_rx_tail+1) % UART_BUF_SIZE;
        num--;
    }

    return 1;
}

int async_uart_send_num(const char *buf, int num)
{
    while (num)
    {
        if (IS_TX_FILL)
            continue;

        uart_tx_rb[uart_tx_head] = *buf++;
        uart_tx_head = (uart_tx_head+1) % UART_BUF_SIZE;
        num--;
        enable_tx_intr();
    }

    return 1;
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

#define MAILBOX_TAG_GET_BOARD_REVISION 0x00010002
#define MAILBOX_TAG_GET_ARM_MEMORY 0x00010005
#define MAILBOX_TAG_REQ_CODE 0
#define MAILBOX_TAG_END 0

#define MAILBOX0_CHANNEL_PROPERTYTAGS_ARMVC 0x8

#define MAILBOX_ALLOC_BUFFER 0x40001

int mailbox_call(uint32_t channel, volatile uint32_t *mbox)
{

    pte_t *pte = (pte_t *)walk(current->mm->pgd, (uint64_t)mbox & ~0xfff);
    if (pte == NULL)
        hangon();

    pte = (void *)(((uint64_t)pte & ~0xfff) + ((uint64_t)mbox & 0xfff));
    uint32_t magic = (channel & MAILBOX_CHANNEL_MASK) |
                     ((uint32_t)(uint64_t)pte & MAILBOX_DATA_MASK);
    /* Wait for mailbox not full */
    while (*MAILBOX0_STATUS & MAILBOX_STATUS_FULL);
    /* Pass message to GPU */
    *MAILBOX1_WRITE = magic;
    /* Wait for mailbox not empty */
    while (*MAILBOX0_STATUS & MAILBOX_STATUS_EMPTY);

    for (int i = 0; i < mbox[0] / 4; i++) {
        if (mbox[i] == MAILBOX_ALLOC_BUFFER) {
            mappages(current->mm->pgd, mbox[i+3], mbox[i+4], mbox[i+3]);
            break;
        }
    }

    return *MAILBOX0_READ == magic;
}

void get_board_revision(uint32_t *frev)
{
    volatile uint32_t __attribute__((aligned(0x10))) mbox_buffer[36];
    mbox_buffer[0] = 7 * 4;
    mbox_buffer[1] = MAILBOX_REQ_CODE_PROC_REQ;
    /* Tags */
    mbox_buffer[2] = MAILBOX_TAG_GET_BOARD_REVISION;
    mbox_buffer[3] = 4; /* Max value buffer size */
    mbox_buffer[4] = MAILBOX_TAG_REQ_CODE;
    mbox_buffer[5] = 0; /* Value buffer */
    mbox_buffer[6] = MAILBOX_TAG_END;

    mailbox_call(MAILBOX0_CHANNEL_PROPERTYTAGS_ARMVC, mbox_buffer);
    *frev = mbox_buffer[5];
}

void get_arm_memory(uint32_t *base, uint32_t *size)
{
    volatile uint32_t __attribute__((aligned(0x10))) mbox_buffer[36];
    mbox_buffer[0] = 8 * 4;
    mbox_buffer[1] = MAILBOX_REQ_CODE_PROC_REQ;
    /* Tags */
    mbox_buffer[2] = MAILBOX_TAG_GET_ARM_MEMORY;
    mbox_buffer[3] = 8; /* Max value buffer size */
    mbox_buffer[4] = MAILBOX_TAG_REQ_CODE;
    mbox_buffer[5] = 0; /* Value buffer [0] */
    mbox_buffer[6] = 0; /* Value buffer [1] */
    mbox_buffer[7] = MAILBOX_TAG_END;

    mailbox_call(MAILBOX0_CHANNEL_PROPERTYTAGS_ARMVC, mbox_buffer);
    *base = mbox_buffer[5];
    *size = mbox_buffer[6];
}
