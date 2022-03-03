#ifndef _UTIL_H_
#define _UTIL_H_

/* TYPE DEFINITIONS */
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long int64_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned uint32_t;
typedef unsigned long uint64_t;

/* REGISTER DATA TYPE DEFINITION */
/**
 * Use "volatile" to make sure before accessing the variable,
 * process always read it from memory instead of register
 */
typedef volatile uint32_t reg32;
typedef volatile uint64_t reg64;

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
int strcmp(const char *s1, const char *s2);

/* OTHER HELPER */
void sleep(int cycles);

#endif /* _UTIL_H_ */