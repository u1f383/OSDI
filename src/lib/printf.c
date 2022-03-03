#include "printf.h"
#include "util.h"

#include <stdarg.h>

/* Only support base 10 and 16 now */
uint64_t strtolu(const char *nptr, int base)
{
    uint64_t value = 0;

    if (base == 10)
    {
        while (*nptr >= '0' && *nptr <= '9')
        {
            value *= 10;
            value += *nptr++ - '0';    
        }
    }
    else if (base == 16)
    {
        while ((*nptr >= '0' && *nptr <= '9') ||
                (*nptr >= 'A' && *nptr <= 'F') || 
                (*nptr >= 'a' && *nptr <= 'f'))
        {
            value *= 16;
            if (*nptr >= '0' && *nptr <= '9')
                value += *nptr++ - '0';
            else if (*nptr >= 'A' && *nptr <= 'F')
                value += *nptr++ - 'A' + 10;
            else
                value += *nptr++ - 'a' + 10;
        }
    }
    return value;
}

/* Only support base 10 and 16 now */
void lutoa(char *nptr, uint64_t value, int base)
{
    if (value == 0) {
        *nptr = '0';
        *(nptr + 1) = '\0';
        return;
    }

    char buf[0x100];
    int idx = 0;
    while (value)
    {
        buf[idx] = value % base;
        if (base == 10 || buf[idx] < 10)
            buf[idx++] += '0';
        else
            buf[idx++] += 'A' - 10;
        value /= base;
    }
    
    int i = 0;
    for (; i < idx; i++)
        *(nptr + i) = buf[idx-1 - i];
    *(nptr + i) = '\0';
}

int vsprintf(char * buf,const char *fmt, va_list args)
{
    return 0;
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
	char buf[0x100];
	int count = 0;
	va_list args;

	va_start(args, fmt);
	count = vsprintf(buf, fmt, args);
	va_end(args);

	return count;
}

/* TODO */