#ifndef _PRINTF_H_
#define _PRINTF_H_

#include "util.h"
#include <stdarg.h>

uint64_t strtolu(const char *nptr, int base);
void lutoa(char *nptr, uint64_t value, int base);
int printf(const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int vsprintf(char * buf,const char *fmt, va_list args);

#endif /* _PRINTF_H_ */