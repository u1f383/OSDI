#ifndef _LIST_H_
#define _LIST_H_

#include <types.h>

#define offsetof(type, member) ((uint64_t) & ((type *)0)->member)
#define container_of(ptr, type, member) ({                    \
        const typeof(((type *)0)->member) *__mptr = (ptr);    \
        (type *)((char *)__mptr - offsetof(type, member));    \
    })

struct list_head {
	struct list_head *next, *prev;
};

#define LIST_POISON1 ((struct list_head *) 0xDEADBEEF)
#define LIST_POISON2 ((struct list_head *) 0xC8763000)

#define LIST_HEAD_INIT(name) (struct list_head) { &(name), &(name) }
#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

#define LIST_INIT(name)                              \
    do                                               \
    {                                                \
        name = (struct list_head) {&(name), &(name)}; \
    } while (0)

static inline void __list_add(
                struct list_head *_new,
                struct list_head *prev,
                struct list_head *next)
{
	next->prev = _new;
	_new->next = next;
	_new->prev = prev;
	prev->next = _new;
}

static inline void list_add(struct list_head *_new, struct list_head *head)
{
    __list_add(_new, head, head->next);
}

static inline void list_add_tail(struct list_head *_new, struct list_head *head)
{
    __list_add(_new, head->prev, head);
}

static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->prev = LIST_POISON1;
    entry->next = LIST_POISON2;
}

#endif /* _LIST_H_ */