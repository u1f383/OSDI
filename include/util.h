#ifndef _UTIL_H_
#define _UTIL_H_

#include <types.h>

/* MEMORY SIZE DEFINITIONS */
#define KB_BIT 10
#define KB (1 << KB_BIT)
#define MB_BIT 20
#define MB (1 << MB_BIT)
#define GB_BIT 30
#define GB (1 << GB_BIT)

/* BIT OPERATION */

/* [low, high) */
#define get_bits(data, low, high) \
    ((data & ((1U << high) - 1U)) >> low)

#define set_bit(data, bit) \
    do { \
        data |= (1U << bit); \
    } while(0)

#define unset_bit(data, bit) \
    do { \
        data &= ~(1U << bit); \
    } while (0)

#define toggle_bit(data, bit) \
    do { \
        data ^= (1U << bit); \
    } while(0)

#define set_zero(data, low, high) \
    do { \
        data &= ~((1U << high) - (1U << low)); \
    } while (0)

#define set_value(data, value, low, high) \
    do { \
        data = ((data) & ~((1U << (high)) - (1U << (low)))) | ((value) << low); \
    } while(0)

/* STRING OPERATION */
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define NULL 0

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, uint32_t cnt);
int strcpy(char *s1, const char *s2);
int strlen(const char *s1);

/* MEMORY OPERATION */
#define ALIGN(x, a)	 (((x) + ((a) - 1)) & ~((a) - 1))
#define PALIGN(p, a) ((void *)(ALIGN((unsigned long)(p), (a))))
int memcmp(const void *dst, const void *src, uint32_t sz);
void memcpy(void *dst, const void *src, uint32_t sz);
void memset(void *s1, char c, uint32_t sz);

/* MATH OPERATION */
uint64_t ceiling_2(uint64_t value);
uint64_t floor_2(uint64_t value);
int8_t log_2(uint64_t value);
#define endian_xchg_32(val) ( ((val>>24)&0xff)     | \
                              ((val>>8)&0xff00)    | \
                              ((val<<8)&0xff0000)  | \
                              ((val<<24)&0xff000000) )

#define endian_xchg_64(val) ( ((val>>56)&0xff)               | \
                              ((val>>40)&0xff00)             | \
                              ((val>>24)&0xff0000)           | \
                              ((val>>8)&0xff000000)          | \
                              ((val<<8)&0xff00000000)        | \
                              ((val<<24)&0xff0000000000)     | \
                              ((val<<40)&0xff000000000000)   | \
                              ((val<<56)&0xff00000000000000) )

#define _floor(val, bit) ( (val) & ~((1 << bit) - 1) )
#define _ceil(val, bit)  ( _floor(val + ((1 << bit) - 1), bit) )
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

/* REGISTER OPERATION */
#define read_sysreg(reg) ({          \
    uint64_t _val;                   \
    __asm__ volatile("mrs x0, " #reg \
                     : "=r"(_val));  \
                    _val; })
#define read_normreg(reg) ({         \
    uint64_t _val;                   \
    __asm__ volatile("mov x0, " #reg \
                     : "=r"(_val));  \
                    _val; })

#define write_sysreg(reg, _val) ({ __asm__ volatile("msr " #reg ", %0" ::"rZ"(_val)); })

/* OTHER HELPER */
void delay(int cycles);
#define hangon() do{ while (1); } while (0)

#endif /* _UTIL_H_ */