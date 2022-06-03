#ifndef _LIB_PRINTF_H_
#define _LIB_PRINTF_H_

#include <util.h>
#include <types.h>
#include <stdarg.h>

typedef void (*PRINT_FPTR)(const char *);

extern PRINT_FPTR __print_func;

void printf_init(PRINT_FPTR pf);
int printf(const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int vsprintf(char *buf, const char *fmt, va_list args);

uint64_t strtolu(const char *nptr, int base);
int64_t strtol(const char *nptr, int base);
uint32_t strtou(const char *nptr, int base);
int32_t strtoi(const char *nptr, int base);

int lutostr(char *nptr, uint64_t value, int base);
int ltostr(char *nptr, int64_t value, int base);
int utostr(char *nptr, uint32_t value, int base);
int itostr(char *nptr, int32_t value, int base);


#endif /* _LIB_PRINTF_H_ */