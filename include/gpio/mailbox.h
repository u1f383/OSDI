#ifndef _GPIO_MAILBOX_H_
#define _GPIO_MAILBOX_H_

#include <gpio/base.h>
#include <types.h>

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

extern volatile uint32_t __attribute__((aligned(0b10000))) mbox_buffer[36];

int mailbox_call(uint32_t channel, volatile uint32_t *mbox);
void get_board_revision(uint32_t *frev);
void get_arm_memory(uint32_t *base, uint32_t *size);

#endif /* _GPIO_MAILBOX_H_ */