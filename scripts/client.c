#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <txrx/txrx.h>

#define COL_RED "\033[31m"
#define COL_GREEN "\033[32m"
#define COL_ORANGE "\033[33m"
#define COL_DEFAULT "\033[39m"

#define BUF_SIZE 0x1000
#define DEV_PATH "/dev/tty.usbserial-0001"
#define KERN_PATH "/tmp/kernel.bin"

char gbuf[BUF_SIZE];

/**
 * Search b in a with restricted size
 */
static inline int search_data(const char *a, uint32_t asz, const char *b, uint32_t bsz)
{
    for (uint32_t i = 0; i < asz; i++)
    {
        if (a[i] == b[0])
        {
            int tmp_i = i + 1, tmp_j = 1;
            while (tmp_i < asz && tmp_j < bsz && a[tmp_i] == b[tmp_j])
            {
                tmp_i++;
                tmp_j++;
            }
            if (tmp_j == bsz)
                return i;
        }
    }

    return -1;
}

typedef enum _LOG_LEVEL
{
    LOG_WARN,
    LOG_SUCCESS,
    LOG_ERR
} LOG_LEVEL;

static void log(LOG_LEVEL log_l, const char *msg)
{
    switch (log_l)
    {
    case LOG_WARN:
        printf(COL_ORANGE "[WARN] %s\n" COL_DEFAULT, msg);
        break;
    case LOG_SUCCESS:
        printf(COL_GREEN "[OK] %s\n" COL_DEFAULT, msg);
        break;
    case LOG_ERR:
        printf(COL_RED "[FAIL] %s\n" COL_DEFAULT, msg);
        break;
    }
}

#define _log(type, level)                          \
    static inline void log_##type(const char *msg) \
    {                                              \
        log(LOG_##level, msg);                     \
    }

_log(warn, WARN);
_log(err, ERR);
_log(success, SUCCESS);

static inline void log_err_exit(const char *msg)
{
    log_err(msg);
    exit(1);
}

#define SER_BUFSIZE 1024
typedef struct _Serial
{
    uint32_t fd;
    char *buf;
    int (*recv)(struct _Serial*, uint32_t);
    int (*recvuntil)(struct _Serial*, const char *);

    int (*send)(struct _Serial*, const char *, uint32_t);
    int (*sendline)(struct _Serial*, const char *, uint32_t);
} Serial;

int ser_recv(Serial *self, uint32_t sz)
{
    int ret = read(self->fd, self->buf,
                   (sz > SER_BUFSIZE) ? SER_BUFSIZE : sz);

    if (ret == -1)
        log_err("serial recv failed");
    return ret;
}

int ser_recvuntil(Serial *self, const char *msg)
{
    int curr = 0;
    int msglen = strlen(msg);

    while (1)
    {
        if ((read(self->fd, self->buf, 1)) != 1)
        {
            log_err("serial recvuntil failed");
            return -1;
        }

        curr += 1;
        if (search_data(self->buf, curr, msg, msglen) != -1)
            return curr;
    }

    return -1;
}

int ser_send(Serial *self, const char *msg, uint32_t sz)
{
    int ret = write(self->fd, self->buf,
                    (sz > SER_BUFSIZE) ? SER_BUFSIZE : sz);

    if (ret == -1)
        log_err("serial send failed");
    return ret;
}

int ser_sendline(Serial *self, const char *msg, uint32_t sz)
{
    char lbuf[SER_BUFSIZE + 1];
    uint32_t wsz = (sz > SER_BUFSIZE) ? SER_BUFSIZE : sz;

    memcpy(lbuf, self->buf, wsz);
    lbuf[wsz] = '\n';

    int ret = write(self->fd, lbuf, wsz);
    if (ret == -1)
        log_err("serial sendline failed");
    return ret;
}

Serial *new_serial(const char *dev_path)
{
    Serial *ser = malloc(sizeof(Serial));

    if ((ser->fd = open(dev_path, O_RDWR | O_NOCTTY | O_NONBLOCK)) == -1)
        goto _new_serial_failed_1;
    if ((ser->buf = malloc(SER_BUFSIZE)) == NULL)
        goto _new_serial_failed_2;

    ser->recv = ser_recv;
    ser->recvuntil = ser_recvuntil;
    ser->send = ser_send;
    ser->sendline = ser_sendline;

    return ser;

_new_serial_failed_2:
    close(ser->fd);
_new_serial_failed_1:
    free(ser);
    return NULL;
}

void release_serial(Serial *ser)
{
    close(ser->fd);
    free(ser->buf);
    free(ser);
}

Packet *new_packet()
{
    Packet *packet = malloc(sizeof(Packet) + UINT16_MAX);
    packet->id = 0;
    packet->checksum = 0;
    packet->metadata = 0;
    packet->size = 0;
    return packet;
}

void release_packet(Packet *packet)
{
    free(packet);
}

int conn_req(Serial *ser)
{
    int ret_sz;
    /* Handshaking */
    if (ser->recvuntil(ser, "# ") == -1      ||
        ser->send(ser, MAGIC_1_STR, 4) == -1 ||
        ser->recv(ser, 4) == -1              ||
        (*(uint32_t *)ser->buf) != MAGIC_2   ||
        ser->send(ser, MAGIC_3_STR, 4) == -1)
    {
        return -1;
    }
    return 0;
}

int tx_file(Serial *ser, const char *filename)
{
    Packet *tx_packet = new_packet();
    Packet *rx_packet = (Packet *) ser->buf;

    int fd;
    if ((fd = open(filename, O_RDONLY)) == -1)
        return -1;

    int rsz;
    while ((rsz = read(fd, tx_packet->data, UINT16_MAX)) != -1)
    {
        tx_packet->size = rsz;
        tx_packet->metadata = PED;
        tx_packet->checksum = calc_checksum(tx_packet->data, tx_packet->size);

    _resend:
        if ((rsz = ser->send(ser, (const char *)tx_packet,
                             sizeof(tx_packet) + tx_packet->size)) == -1)
            break;
        if ((rsz = ser->recv(ser, sizeof(Packet))) != -1)
            break;

        if (!is_ok(rx_packet))
            goto _resend;

        tx_packet->id++;
    }

    if (rsz == 0) {
        tx_packet->metadata = END;
        rsz = ser->send(ser, (const char *)tx_packet, sizeof(tx_packet));
        if (rsz == -1)
            log_warn("TX END packet failed");
    }

    close(fd);
    release_packet(tx_packet);
    return rsz;    
}

int conn_end(Serial *ser)
{
    Packet *tx_packet = new_packet();
    Packet *rx_packet = (Packet *) ser->buf;
    int retval;

    tx_packet->metadata = END;
    if ((retval = ser->send(ser, (const char *) ser, sizeof(Packet))) == -1)
        goto _conn_end_failed;
    
    if ((retval = ser->recv(ser, sizeof(Packet))) == -1)
        goto _conn_end_failed;

    retval = !is_finished(rx_packet) ? -1 : 0;
        
_conn_end_failed:
    release_packet(tx_packet);
    return retval;
}

int main()
{
    Serial *ser;
    if ((ser = new_serial(DEV_PATH)) == NULL)
        log_err_exit("open serial failed");

    if (conn_req(ser)) {
        log_err("handshake failed");
        goto _main_exit;
    }
    log_success("handshake done");

    if (tx_file(ser, KERN_PATH)) {
        log_err("transmit file failed");
        goto _main_exit;
    }
    conn_end(ser);

_main_exit:
    release_serial(ser);
    ser = NULL;

    return 0;
}