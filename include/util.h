#ifndef _UTIL_H_
#define _UTIL_H_

#include <types.h>

/* MEMORY SIZE DEFINITIONS */
#define KB (1 << 10)
#define MB (KB << 10)
#define GB (MB << 10)

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
int strcpy(char *s1, const char *s2);

/* MEMORY OPERATION */
int memcmp(const char *dst, const char *src, uint32_t sz);
void memcpy(char *dst, const char *src, uint32_t sz);
void memset(char *s1, char c, uint32_t sz);

/* MATH OPERATION */
uint64_t ceiling_2(uint64_t value);
uint8_t log_2(uint64_t value);

/* OTHER HELPER */
void sleep(int cycles);

#endif /* _UTIL_H_ */