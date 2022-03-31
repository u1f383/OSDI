#include <gpio/uart.h>
#include <gpio/gpio.h>
#include <interrupt/irq.h>
#include <util.h>

#define UART_BUF_SIZE 0x100

AuxRegs *aux_regs;
char uart_rbuf[ UART_BUF_SIZE ];
char uart_wbuf[ UART_BUF_SIZE ];
int32_t uart_rbuf_top = 0;
int32_t uart_rbuf_cur = 0;
int32_t uart_wbuf_top = 0;
int32_t uart_wbuf_cur = 0;

static inline int is_wbuf_empty()
{
    return uart_wbuf_top == uart_wbuf_cur;
}

static inline int is_wbuf_fill()
{
    return uart_wbuf_top == uart_wbuf_cur-1 ||
           uart_wbuf_top == uart_wbuf_cur - UART_BUF_SIZE + 1;
}

static inline int is_rbuf_empty()
{
    return uart_rbuf_top == uart_rbuf_cur;
}

static inline int is_rbuf_fill()
{
    return uart_rbuf_top == uart_rbuf_cur-1 ||
           uart_rbuf_top == uart_rbuf_cur - UART_BUF_SIZE + 1;
}

void uart_init()
{
    aux_regs = ((AuxRegs *) AUX_PERIF_REG_MAP_BASE);

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
    sleep(150);
    /* Send the modification signal to GPIO14/15 */
    set_value(*GPIO_GPPUDCLK0_REG, 0b11, 14, 14 + 2);
    sleep(150);

    /**
     * Remove control signal by writing gppud, and
     * remove clock by writing GPPUDCLK0
     */
    set_value(*GPIO_GPPUD_REG, GPIO_GPPUD_Off, 0, sizeof(reg32));
    set_value(*GPIO_GPPUDCLK0_REG, GPIO_GPPUDCLK_NO_EFFECT, 0, sizeof(reg32));
}

void uart_eint()
{
    /* Enable IRQs1 - Aux int */
    uint32_t *int_reg_irqs1 = (uint32_t *) ARM_INT_IRQs1_REG;
    *int_reg_irqs1 |= (1 << 29);
    
    /* Enable recv interrupt */
    set_value(aux_regs->mu_ier, 1, AUXMUIER_Enable_receive_interrupts_BIT,
                                    AUXMUIER_Enable_transmit_interrupts_BIT);
}

void uart_intr_handler(reg32 orig_ier)
{
    reg32 orig_iir = aux_regs->mu_iir;

    /* Transmit holding register empty */
    if (orig_iir & 0b010)
    {
        if (!is_wbuf_empty())
        {
            set_value(aux_regs->mu_io, uart_wbuf[ uart_wbuf_cur ], AUXMUIO_Transmit_data_write_BIT, AUXMUIO_RESERVED_BIT);
            uart_wbuf_cur = (uart_wbuf_cur+1) % UART_BUF_SIZE;
        }
        else
            orig_ier &= ~(0b10);
    }
    /* Receiver holds valid byte */
    else if ((orig_iir & 0b100) && !is_rbuf_fill())
    {
        uart_rbuf[ uart_rbuf_top ] = get_bits(aux_regs->mu_io, AUXMUIO_Receive_data_read_BIT, AUXMUIO_RESERVED_BIT);
        uart_rbuf_top = (uart_rbuf_top+1) % UART_BUF_SIZE;
    }

    set_value(aux_regs->mu_iir, 0, AUXMUIIR_FIFO_clear_bits_BIT, AUXMUIIR_ALWAYS_ZERO_BIT); // maybe not need
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

/**
 * =====================================
 *             async handle
 * =====================================
 */

void async_uart_sendchr(char c)
{
    if (!is_wbuf_fill())
    {
        uart_wbuf[ uart_wbuf_top ] = c;
        uart_wbuf_top = (uart_wbuf_top+1) % UART_BUF_SIZE;
        set_value(aux_regs->mu_ier, aux_regs->mu_ier | 0b10,
                AUXMUIER_Enable_receive_interrupts_BIT, AUXMUIER_RESERVED_BIT);
    }
}

void async_uart_sendstr(const char *str)
{
    if (*str == '\0' || is_wbuf_fill())
        return;

    while (!is_wbuf_fill() && *str)
    {
        uart_wbuf[ uart_wbuf_top ] = *str++;
        uart_wbuf_top = (uart_wbuf_top+1) % UART_BUF_SIZE;
    }

    set_value(aux_regs->mu_ier, aux_regs->mu_ier | 0b10,
                AUXMUIER_Enable_receive_interrupts_BIT, AUXMUIER_RESERVED_BIT);
}

void async_uart_cmd(char *ptr)
{
    while (1)
    {
        if (is_rbuf_empty())
            continue;
        
        *ptr = uart_rbuf[ uart_rbuf_cur ];
        uart_rbuf_cur = (uart_rbuf_cur+1) % UART_BUF_SIZE;
        
        if (*ptr == '\r' || *ptr == '\n')
            break;
        
        async_uart_sendchr(*ptr);
        ptr++;
    }
    *ptr = '\0';
}