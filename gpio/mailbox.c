#include <gpio/mailbox.h>

volatile uint32_t __attribute__((aligned(0b10000))) mbox_buffer[36];

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