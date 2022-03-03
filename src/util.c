#include "util.h"

int strcmp(const char *s1, const char *s2) {
    while (*s1 != '\0' && *s2 != '\0' && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

__asm__(
    ".global sleep\n"
    "sleep:\n"
    "subs x0, x0, #1\n"
    "cbnz x0, sleep\n"
    "ret\n"
);