#ifndef _TXRX_TXRX_H_
#define _TXRX_TXRX_H_

/**
@ Transmission Protocol

@@ Packet Format
1. id (4 bytes): the # of data
2. status (2 byte):
    - FIN (1): finish
    - PED (2): pending
    - END (3): send end
    - OK  (4): only set by loader, means it receives the correct data 
3. checksum (4 byte)
4. size (2 bytes): data size
5. data (1 ~ 1024)

@@ Transmission Flow
A is server, B is client
1. A ---<magic number 0x54875487>--> B
2. A <--<magic number 0xcccc8763>--- B
3. A ---<magic number 0x77777777>--> B
----------- START transmit -----------
4. A <--------< PED Packet >-------- B
5. A ---------< OK Packet >--------> B
---  if failed, B will resend data ---
6. A <--------< END Packet >-------- B
7. A --------< FIN packet >--------> B
------------ END transmit ------------
**/
#define MAGIC_1 0x54875487
#define MAGIC_1_STR "\x87\x54\x87\x54"
#define MAGIC_2 0x77777777
#define MAGIC_2_STR "\x77\x77\x77\x77"
#define MAGIC_3 0xcccc8763
#define MAGIC_3_STR "\x63\x87\xcc\xcc"
#define FAILED_STR "\xef\xbe\xad\xde"
#define FAILED  0xdeadbeef

typedef struct _Packet
{
    uint32_t id;
    uint8_t status;
    uint8_t checksum;
    uint16_t size;
    char data[0];
} Packet;

#define STATUS_FIN 1
#define STATUS_PED 2
#define STATUS_END 3
#define STATUS_OK  4
#define STATUS_BAD 5

#define is_pending(packet) (((Packet *)packet)->status & PED)
#define is_finished(packet) (((Packet *)packet)->status & FIN)
#define is_ok(packet) (((Packet *)packet)->status & OK)

Packet *new_packet();
void release_packet(Packet *packet);

static inline uint8_t calc_checksum(const unsigned char *data, uint32_t sz)
{
    uint8_t sum = 0;
    for (int i = 0; i < sz; i++)
        sum += data[i];
    return sum;
}

#endif /* _TXRX_TXRX_H_ */