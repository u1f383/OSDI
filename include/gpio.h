#ifndef _GPIO_H_
#define _GPIO_H_

#include <util.h>
#include <types.h>
#include <fs.h>

/** 
 * Device tree of bcm2837 board:
 * https://github.com/raspberrypi/linux/blob/rpi-5.15.y/arch/arm/boot/dts/bcm2837.dtsi
 */
#define PERIF_ADDRESS     0xFFFF00003F000000
#define PERIF_ADDRESS_END (PERIF_ADDRESS + ((16 * MB) - 1)
#define PERIF_BUS_ADDRESS 0xFFFF00007E000000
#define PERIF_BUS_ADDRESS_END (PERIF_BUS_ADDRESS + ((16 * MB) - 1))

/**
 * Auxiliary peripherals Register Map (BCM2835 P.8)
 */
#define AUX_PERIF_REG_MAP_BASE (PERIF_ADDRESS + 0x215000)

typedef reg32 AuxIrqReg;
#define AUXIRQ_RESERVED_BIT 3
#define AUXIRQ_SPI2_IRQ_BIT 2
#define AUXIRQ_SPI1_IRQ_BIT 1
#define AUXIRQ_Mini_UART_IRQ_BIT 0

typedef reg32 AuxEnbReg;
#define AUXENB_RESERVED_BIT 3
#define AUXENB_SPI2_enable_BIT 2
#define AUXENB_SPI1_enable_BIT 1
#define AUXENB_Mini_UART_enable_BIT 0

typedef reg32 AuxMuIOReg;
#define AUXMUIO_RESERVED_BIT 8
#define AUXMUIO_Transmit_data_write_BIT 0
#define AUXMUIO_Receive_data_read_BIT 0
#define AUXMUIO_LS_8bits_Baudrate_BIT 0

typedef reg32 AuxMuIerReg;
#define AUXMUIER_RESERVED_BIT 8
#define AUXMUIER_Enable_transmit_interrupts_BIT 1
#define AUXMUIER_Enable_receive_interrupts_BIT 0
#define AUXMUIER_MS_8bits_Baudrate_BIT 0

typedef reg32 AuxMuIirReg;
#define AUXMUIIR_RESERVED_BIT 8
#define AUXMUIIR_FIFO_enables_BIT 6
#define AUXMUIIR_ALWAYS_ZERO_BIT 3
#define AUXMUIIR_FIFO_clear_bits_BIT 1
#define AUXMUIIR_Interrupt_ID_bits_BIT 1
#define AUXMUIIR_Interrupt_pending_BIT 0

typedef reg32 AuxMuLcrRrg;
#define AUXMULCR_RESERVED_BIT 8
#define AUXMULCR_DLAB_access_BIT 7
#define AUXMULCR_Break_BIT 6
#define AUXMULCR_ALWAYS_BIT 2
#define AUXMULCR_data_size_BIT 0

typedef reg32 AuxMuMcrReg;
#define AUXMUMCR_RESERVED_BIT 2
#define AUXMUMCR_RTS_BIT 1

typedef reg32 AuxMuLsrReg;
#define AUXMULSR_RESERVED_BIT 7
#define AUXMULSR_Transmitter_idle_BIT 6
#define AUXMULSR_Transmitter_empty_BIT 5
#define AUXMULSR_ALWAYS_ZERO_BIT 2
#define AUXMULSR_Receiver_Overrun_BIT 1
#define AUXMULSR_Data_ready_BIT 0

typedef reg32 AuxMuMsrReg;
#define AUXMUMSR_RESERVED_BIT 6
#define AUXMUMSR_CTS_status_BIT 5

typedef reg32 AuxMuScratchReg;
#define AUXMUSCRATCH_RESERVED_BIT 8
#define AUXMUSCRATCH_Scratch_BIT 0

typedef reg32 AuxMuCntlReg;
#define AUXMUCNTL_RESERVED_BIT 8
#define AUXMUCNTL_CTS_assert_level 7
#define AUXMUCNTL_RTS_assert_level 6
#define AUXMUCNTL_RTS_AUTO_flow_level_BIT 4
#define AUXMUCNTL_Enable_transmit_Auto_FC_using_CTS_BIT 3
#define AUXMUCNTL_Enable_receive_Auto_FC_using_RTS_BIT 2
#define AUXMUCNTL_Transmitter_enable_BIT 1
#define AUXMUCNTL_Receiver_enable_BIT 0

typedef reg32 AuxMuStatReg;
#define AUXMUSTAT_RESERVED_BIT 28
#define AUXMUSTAT_Transmit_FIFO_fill_level_BIT 24
#define AUXMUSTAT_ALWAYS_ZERO_2_BIT 20
#define AUXMUSTAT_Receive_FIFO_fill_level_BIT 16
#define AUXMUSTAT_ALWAYS_ZERO_BIT 10
#define AUXMUSTAT_Transmitter_done_BIT 9
#define AUXMUSTAT_Transmit_FIFO_is_empty_BIT 8
#define AUXMUSTAT_CTS_line_BIT 7
#define AUXMUSTAT_RTS_status_BIT 6
#define AUXMUSTAT_Transmit_FIFO_is_full_BIT 5
#define AUXMUSTAT_Receiver_overrun_BIT 4
#define AUXMUSTAT_Transmitter_is_idle_BIT 3
#define AUXMUSTAT_Receiver_is_idle_BIT 2
#define AUXMUSTAT_Space_available_BIT 1
#define AUXMUSTAT_Symbol_available_BIT 0

typedef reg32 AuxMuBaudReg;
#define AUXMUBAUD_RESERVED_BIT 16
#define AUXMUBAUD_Baudrate_BIT 0

struct _AuxRegs
{
    /* Auxiliary Interrupt status (0x0) */
    AuxIrqReg irq;
    /* Auxiliary enables (0x4) */
    AuxEnbReg enb;
    uint32_t reserved1[14];
    /* Mini Uart I/O Data (0x40) */
    AuxMuIOReg mu_io;
    /* Mini Uart Interrupt Enable (0x44) */
    AuxMuIerReg mu_ier;
    /* Mini Uart Interrupt Identify (0x48) */
    AuxMuIirReg mu_iir;
    /* Mini Uart Line Control (0x4C) */
    AuxMuLcrRrg mu_lcr;
    /* Mini Uart Modem Control (0x50) */
    AuxMuMcrReg mu_mcr;
    /* Mini Uart Line Status (0x54) */
    AuxMuLsrReg mu_lsr;
    /* Mini Uart Modem Status (0x58) */
    AuxMuMsrReg mu_msr;
    /* Mini Uart Scratch (0x5C) */
    AuxMuScratchReg mu_scratch;
    /* Mini Uart Extra Control (0x60) */
    AuxMuCntlReg mu_cntl;
    /* Mini Uart Extra Status (0x64) */
    AuxMuStatReg mu_stat;
    /* Mini Uart Baudrate (0x68) */
    AuxMuBaudReg mu_baud;

    uint32_t reserved2[5];
    /* SPI 1 Control register 0 (0x80) */
    reg32 spi0_cntl0;
    /* SPI 1 Control register 1 (0x84) */
    reg32 spi0_cntl1;
    /* SPI 1 Status (0x88) */
    uint32_t reserved3[1];
    reg32 spi0_stat;
    /* SPI 1 Data (0x90) */
    reg32 spi0_io;
    /* SPI 1 Peek (0x94) */
    reg32 spi0_peek;
    uint32_t reserved4[10];
    /* SPI 2 Control register 0 (0xC0) */
    reg32 spi1_cntl0;
    /* SPI 2 Control register 1 (0xC4) */
    reg32 spi1_cntl1;
    /* SPI 2 Status (0xC8) */
    reg32 spi1_stat;
    uint32_t reserved5[1];
    /* SPI 2 Data (0xD0) */
    reg32 spi1_io;
    /* SPI 2 Peek (0xD4) */
    reg32 spi1_peek;
} __attribute__((__packed__));
typedef struct _AuxRegs AuxRegs;

/**
 * ============ GPIO ============
 */
#define GPIO_PERIF_REG_MAP_BASE (PERIF_ADDRESS + 0x200000)

#define GPIO_GPFSEL0_REG ((reg32 *)(GPIO_PERIF_REG_MAP_BASE + 0x0))
#define GPIO_GPFSEL1_REG ((reg32 *)(GPIO_PERIF_REG_MAP_BASE + 0x4))
#define GPIO_GPFSEL2_REG ((reg32 *)(GPIO_PERIF_REG_MAP_BASE + 0x8))
#define GPIO_GPFSEL3_REG ((reg32 *)(GPIO_PERIF_REG_MAP_BASE + 0xC))
#define GPIO_GPFSEL4_REG ((reg32 *)(GPIO_PERIF_REG_MAP_BASE + 0x10))
#define GPIO_GPFSEL5_REG ((reg32 *)(GPIO_PERIF_REG_MAP_BASE + 0x14))
#define GPIO_FSEL_SIZE  3
#define GPIO_FSEL14_BIT 12 /* GPIO 14 PIN */
#define GPIO_FSEL15_BIT 15 /* GPIO 15 PIN */

#define GPIO_GPSET0_REG ((reg32 *)(GPIO_PERIF_REG_MAP_BASE + 0x1C))
#define GPIO_GPSET1_REG ((reg32 *)(GPIO_PERIF_REG_MAP_BASE + 0x20))

#define GPIO_GPCLR0_REG ((reg32 *)(GPIO_PERIF_REG_MAP_BASE + 0x28))

#define GPIO_GPLEV0_REG ((reg32 *)(GPIO_PERIF_REG_MAP_BASE + 0x34))
#define GPIO_GPLEV1_REG ((reg32 *)(GPIO_PERIF_REG_MAP_BASE + 0x38))

#define GPIO_GPEDS0_REG ((reg32 *)(GPIO_PERIF_REG_MAP_BASE + 0x40))
#define GPIO_GPEDS1_REG ((reg32 *)(GPIO_PERIF_REG_MAP_BASE + 0x44))

#define GPIO_GPHEN0_REG ((reg32 *)(GPIO_PERIF_REG_MAP_BASE + 0x64))
#define GPIO_GPHEN1_REG ((reg32 *)(GPIO_PERIF_REG_MAP_BASE + 0x68))

#define GPIO_GPPUD_REG ((reg32 *)(GPIO_PERIF_REG_MAP_BASE + 0x94))
#define GPIO_GPPUDCLK0_REG ((reg32 *)(GPIO_PERIF_REG_MAP_BASE + 0x98))
#define GPIO_GPPUDCLK1_REG ((reg32 *)(GPIO_PERIF_REG_MAP_BASE + 0x9C))
#define GPIO_GPPUD_Off 0b00
#define GPIO_GPPUD_Enable_Pull_Down 0b01
#define GPIO_GPPUD_Enable_Pull_Up 0b10
#define GPIO_GPPUDCLK_NO_EFFECT 0

/**
 * ============ UART ============
 */

/* Default core clock is fixed to 250 MHz */
#define VPU_SYSTEM_CLOCK_FREQ (250 * 1000 * 1000)
#define BAUDRATE 115200
#define BAUDRATE_REG ((VPU_SYSTEM_CLOCK_FREQ / (8 * BAUDRATE)) - 1) // 271

#define ARM_INT_REG_BASE (PERIF_ADDRESS + 0xB000)
#define ARM_INT_IRQs1_REG (ARM_INT_REG_BASE + 0x210)

extern AuxRegs *aux_regs;

void uart_init();
void uart_intr_handler(reg32 orig_ier);

void uart_send(char c);
void uart_send_num(char *buf, int num);
void uart_recvline(char *ptr);
char uart_recv();
void uart_recv_num(char *buf, int num);
void uart_sendstr(const char *str);
void uart_cmd(char *ptr);

int async_uart_recv_num(char *buf, int num);
int async_uart_send_num(const char *buf, int num);
void async_uart_sendstr(const char *str);

static inline void disable_uart()
{
    set_value(aux_regs->mu_ier, 0, AUXMUIER_Enable_receive_interrupts_BIT, AUXMUIER_RESERVED_BIT);
}

static inline void enable_rx_intr()
{
    set_value(aux_regs->mu_ier, aux_regs->mu_ier | 1,
            AUXMUIER_Enable_receive_interrupts_BIT, AUXMUIER_RESERVED_BIT);    
}

static inline void disable_rx_intr()
{
    set_value(aux_regs->mu_ier, aux_regs->mu_ier & ~1,
            AUXMUIER_Enable_receive_interrupts_BIT, AUXMUIER_RESERVED_BIT);    
}

static inline void enable_tx_intr()
{
    set_value(aux_regs->mu_ier, aux_regs->mu_ier | 0b10,
            AUXMUIER_Enable_receive_interrupts_BIT, AUXMUIER_RESERVED_BIT);    
}

static inline void disable_tx_intr()
{
    set_value(aux_regs->mu_ier, aux_regs->mu_ier & ~0b10,
            AUXMUIER_Enable_receive_interrupts_BIT, AUXMUIER_RESERVED_BIT);    
}

static inline void uart_enable_intr()
{
    /* Enable IRQs1 - Aux int */
    uint32_t *int_reg_irqs1 = (uint32_t *)ARM_INT_IRQs1_REG;
    *int_reg_irqs1 |= (1 << 29);
    enable_rx_intr();
}

/**
 * ============ Mailbox ============
 */

int mailbox_call_wrapper(uint32_t channel, volatile uint32_t *_mbox);
void mailbox_init(struct vnode *vnode);

/**
 * ============ file operation ============
 */
extern const struct file_operations uart_file_ops;
extern const struct file_operations fb_file_ops;

#endif /* _GPIO_H_ */