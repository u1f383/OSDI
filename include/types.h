#ifndef _TYPES_H_
#define _TYPES_H_

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

typedef uint32_t uid_t;
typedef uint32_t gid_t;

typedef uint64_t addr_t;
typedef uint64_t time64_t;
typedef uint32_t dev_t;

#endif /* _TYPES_H_ */