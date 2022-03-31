#include <util.h>
#include <types.h>

int strlen(const char *s1)
{
    const char *s2 = s1;
    while(*s1++);

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
    if (value == 0)
        return 0;

    int cnt = 0;
    int _bit = -1;

    while (value)
    {
        if (value & 1)
            cnt++;
        
        _bit++;
        value >>= 1;
    }

    if (cnt > 1)
        return (1 << (_bit + 1));
    
    return 1 << _bit;
}

uint8_t log_2(uint64_t value)
{
    if (value == 0)
        return 0;

    uint8_t _bit = 0;
    while (value)
    {
        _bit++;
        value >>= 1;
    }
    return _bit - 1;
}