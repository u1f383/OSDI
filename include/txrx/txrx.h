#ifndef _TXRX_TXRX_H_
#define _TXRX_TXRX_H_

#include <util.h>

/**
@ Transmission Protocol

@@ Packet Format
1. id (4 bytes): the # of data
2. metadata (1 byte):
    - FIN (bit 0): finish bit
    - PED (bit 1): pending bit
    - OK (bit 2): only set by loader, means it receives the correct data 
3. checksum (1 byte)
4. size (2 bytes): data size, if 0 it means the transmission has done
5. data (1 ~ 65536)

@@ Transmission Flow
A is server, B is client
1. A <--<magic number 0x54875487>--- B
2. A ---<magic number 0xcccc8763>--> B
3. A <--<magic number 0x77777777>--- B
----------- START transmit -----------
4. A <--------< PED Packet >-------- B
5. A --< OK packet (per 65536) >---> B
---  if failed, B will resend data ---
6. A <-----< FIN packet * 3 >------- B
7. A ------< FIN packet * 3 >------> B
------------ END transmit ------------
**/
#define MAGIC_1 0x54875487
#define MAGIC_1_STR "\x87\x54\x87\x54"
#define MAGIC_2 0x77777777
#define MAGIC_2_STR "\x77\x77\x77\x77"
#define MAGIC_3 0xcccc8763
#define MAGIC_3_STR "\x63\x87\xcc\xcc"

typedef struct _Packet
{
    uint32_t id;
    uint8_t metadata;
    uint8_t checksum;
    uint16_t size;
    char data[0];
} Packet;

#define FIN 0b00000001
#define PED 0b00000010
#define OK  0b00000100

#define is_pending(packet) (packet->metadata & PED)
#define is_finished(packet) (packet->metadata & FIN)
#define is_ok(packet) (packet->metadata & OK)

#endif /* _TXRX_TXRX_H_ */