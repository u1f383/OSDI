#include <lib/printf.h>
#include <util.h>
#include <stdarg.h>

PRINT_FPTR __print_func;

void printf_init(PRINT_FPTR pf)
{
    __print_func = pf;
}

/* Only support base 10 and 16 now */
#define strto(_name_, _type_)                          \
    strto##_name_(const char *nptr, int base)          \
    {                                                  \
        _type_ value = 0;                              \
        if (base == 10)                                \
        {                                              \
            while (*nptr >= '0' && *nptr <= '9')       \
            {                                          \
                value *= 10;                           \
                value += *nptr++ - '0';                \
            }                                          \
        }                                              \
        else if (base == 16)                           \
        {                                              \
            while ((*nptr >= '0' && *nptr <= '9') ||   \
                   (*nptr >= 'A' && *nptr <= 'F') ||   \
                   (*nptr >= 'a' && *nptr <= 'f'))     \
            {                                          \
                value *= 16;                           \
                if (*nptr >= '0' && *nptr <= '9')      \
                    value += *nptr++ - '0';            \
                else if (*nptr >= 'A' && *nptr <= 'F') \
                    value += *nptr++ - 'A' + 10;       \
                else                                   \
                    value += *nptr++ - 'a' + 10;       \
            }                                          \
        }                                              \
        return value;                                  \
    }

uint64_t strto(lu, uint64_t);
int64_t strto(l, int64_t);
uint32_t strto(u, uint32_t);
int32_t strto(i, int64_t);

/* Only support base 10 and 16 now */
#define tostr(_name_, _type_)                         \
    _name_##tostr(char *nptr, _type_ value, int base) \
    {                                                 \
        if (value == 0)                               \
        {                                             \
            *nptr = '0';                              \
            *(nptr + 1) = '\0';                       \
            return 1;                                 \
        }                                             \
        if (value < 0)                                \
        {                                             \
            *nptr++ = '-';                            \
            value *= -1;                              \
        }                                             \
                                                      \
        char buf[0x100];                              \
        int idx = 0;                                  \
        while (value)                                 \
        {                                             \
            buf[idx] = value % base;                  \
            if (base == 10 || buf[idx] < 10)          \
                buf[idx++] += '0';                    \
            else                                      \
                buf[idx++] += 'A' - 10;               \
            value /= base;                            \
        }                                             \
                                                      \
        int i = 0;                                    \
        for (; i < idx; i++)                          \
            *(nptr + i) = buf[idx - 1 - i];           \
        *(nptr + i) = '\0';                           \
                                                      \
        return i;                                     \
    }

int tostr(lu, uint64_t);
int tostr(l, int64_t);
int tostr(u, uint32_t);
int tostr(i, int32_t);

/**
 * Only support some common formats:
 *  %s: string
 *  %p: ptr
 *  %x: hex
 *  %d: signed digit
 *  %u: unsigned digit
 *  %lx: long hex
 *  %ld: long signed digit
 *  %lu: long unsigned digit
 */
int vsprintf(char *buf, const char *fmt, va_list args)
{
    int _long = 0;
    int count = 0;
    int base;
    char ch;

    while ((ch = *fmt++))
    {
        _long = 0;
        switch (ch)
        {
        case '%':
            ch = *fmt++;
            if (ch == 'l')
            {
                _long = 1;
                ch = *fmt++;
            }

            if (ch == 'u' || ch == 'x')
            {
                base = ch == 'x' ? 16 : 10;
                if (_long)
                    buf += lutostr(buf, va_arg(args, uint64_t), base);
                else
                    buf += utostr(buf, va_arg(args, uint32_t), base);
            }
            else if (ch == 'd')
            {
                if (_long)
                    buf += ltostr(buf, va_arg(args, int64_t), 10);
                else
                    buf += itostr(buf, va_arg(args, int32_t), 10);
            }
            else if (ch == '%')
            {
                *buf++ = ch;
            }
            else if (ch == 'p')
            {
                uint64_t addr = (uint64_t)va_arg(args, void *);
                buf += strcpy(buf, "0x");
                buf += lutostr(buf, addr, 16);
            }
            else if (ch == 's')
            {
                buf += strcpy(buf, va_arg(args, char *));
            }
            else /* Not match */
                break;

            count++;
            break;
        default:
            *buf++ = ch;
            break;
        }
    }

    return count;
}

int sprintf(char *buf, const char *fmt, ...)
{
    int count = 0;
    va_list args;

    va_start(args, fmt);
    count = vsprintf(buf, fmt, args);
    va_end(args);

    return count;
}

int printf(const char *fmt, ...)
{
    char buf[0x100] = {0};
    int count = 0;
    va_list args;

    va_start(args, fmt);
    count = vsprintf(buf, fmt, args);
    va_end(args);

    __print_func(buf);
    return count;
}