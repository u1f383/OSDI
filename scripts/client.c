#include <unistd.h>
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
    int (*recv)(struct _Serial, uint32_t);
    int (*recvuntil)(struct _Serial, const char *);

    int (*send)(struct _Serial, const char *, uint32_t);
    int (*sendline)(struct _Serial, const char *, uint32_t);
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
        if (search_data(self->fd, curr, msg, msglen) != -1)
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

int handshake_client(Serial *ser)
{
    int ret_sz;
    char *lbuf = malloc(sizeof(Packet) + UINT16_MAX);
    

    free(lbuf);
    return 0;
}

int main()
{
    Serial *ser;
    if ((ser = new_serial(DEV_PATH)) == NULL)
        log_err_exit("open serial failed");

    if (handshake_client(ser))
        log_err_exit("handshake failed");

    log_success("handshake done");

    release_serial(ser);
    ser = NULL;

    return 0;
}