#include <util.h>
#include <types.h>

int strlen(const char *s1)
{
    const char *s2 = s1;
    while(*s1)
        s1++;

    return s1 - s2;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 != '\0' && *s2 != '\0' && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

int strncmp(const char *s1, const char *s2, uint32_t cnt)
{
    while (*s1 != '\0' && *s2 != '\0' && *s1 == *s2 && cnt--) {
        s1++;
        s2++;
    }
    
    if (cnt == 0)
        return 0;
    
    return *s1 - *s2;
}

int strcpy(char *s1, const char *s2)
{
    const char *org = s2;
    while (*s2) {
        *s1 = *s2;
        s1++;
        s2++;
    }
    *s1 = *s2;
    return s2 - org;
}

int memcmp(const char *s1, const char *s2, uint32_t sz)
{
    while (sz-- && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

void memcpy(char *s1, const char *s2, uint32_t sz)
{
    while (sz--) {
        *s1 = *s2;
        s1++;
        s2++;
    }
}

void memset(char *s1, char c, uint32_t sz)
{
    while (sz--)
        *s1++ = c;
}

__asm__(
    ".global sleep\n"
    "sleep:\n"
    "subs x0, x0, #1\n"
    "cbnz x0, sleep\n"
    "ret\n"
);

uint64_t ceiling_2(uint64_t value)
{
    int8_t _bit;
    if ((_bit = log_2(value)) == -1)
        return 0;
    
    return _ceil(value, _bit);
}

uint64_t floor_2(uint64_t value)
{
    int8_t _bit;
    if ((_bit = log_2(value)) == -1)
        return 0;
    
    return _floor(value, _bit);
}

int8_t log_2(uint64_t value)
{
    int8_t _bit = 0;
    while (value)
    {
        _bit++;
        value >>= 1;
    }
    return _bit - 1;
}