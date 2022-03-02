#include "gpio/uart.h"
#include "gpio/gpio.h"

void uart_init()
{
    /* Update GPIO14/15 to alt5 because of minu uart */
    set_value(*GPIO_GPFSEL1_REG, 0, GPIO_FSEL14_BIT, GPIO_FSEL14_BIT + GPIO_FSEL_SIZE);
    set_value(*GPIO_GPFSEL1_REG, 0, GPIO_FSEL15_BIT, GPIO_FSEL15_BIT + GPIO_FSEL_SIZE);
    
    /* Disable pull-up/down control */
    set_value(*GPIO_GPPUD_REG, GPIO_GPPUD_Off, 0, sizeof(reg32));
    set_value(*GPIO_GPPUDCLK0_REG, GPIO_GPPUDCLK_NO_EFFECT, 0, sizeof(reg32));

    AuxRegs *aux_regs = ((AuxRegs *) AUX_PERIF_REG_MAP_BASE);
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
}

void uart_send(char c)
{

}

void uart_sendstr(char *str)
{

}

