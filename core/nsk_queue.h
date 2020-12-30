

#ifndef _QUEUE_H_2020123
#define _QUEUE_H_2020123
#include <stddef.h>

struct nsk_queue_node_s;
typedef nsk_queue_node_s nsk_queue_node_t;
typedef nsk_queue_node_t nsk_queue_t;

struct nsk_queue_node_s {
    nsk_queue_node_t *prev;
    nsk_queue_node_t *next;
};

#define nsk_queue_init(q) (q)->prev = (q)->next = (q)

#define nsk_queue_prev(q) (q)->prev

#define nsk_queue_next(q) (q)->next

#define nsk_queue_head(q) (q)->next

#define nsk_queue_last(q) (q)->prev

#define nsk_queue_empty(q) ((q) == (q)->prev)

#define nsk_queue_sentinel(q) (q)

#define nsk_queue_insert_head(q, n)                                                                     \
    (n)->next = (q)->next;                                                                              \
    (n)->next->prev = (n);                                                                              \
    (n)->prev = (q);                                                                                    \
    (q)->next = (n)

#define nsk_queue_insert_tail(q, n)                                                                     \
    (n)->prev = (q)->prev;                                                                              \
    (n)->prev->next = (n);                                                                              \
    (n)->next = (q);                                                                                    \
    (q)->prev = (n)

#define nsk_queue_remove(n)                                                                             \
    (n)->prev->next = (n)->next;                                                                        \
    (n)->next->prev = (n)->prev;                                                                        \
    (n)->next = (n)->prev = NULL

#define nsk_queue_merge(q, n)                                                                           \
    if (!nsk_queue_empty(n)) {                                                                          \
        (q)->prev->next = (n)->next;                                                                    \
        (n)->next->prev = (q)->prev;                                                                    \
        (q)->prev = (n)->prev;                                                                          \
        (n)->prev->next = (q);                                                                          \
        nsk_queue_init(n);                                                                               \
    }

#define nsk_queue_spilt(q, h, n)                                                                        \
    (h)->next = (n);                                                                                    \
    (h)->prev = (q)->prev;                                                                              \
    (n)->prev->next = (q);                                                                              \
    (q)->prev->next = (h);                                                                              \
    (q)->prev = (n)->prev;                                                                              \
    (n)->prev = (h)

#define nsk_queue_move_head(d, s)                                                                       \
    (s)->prev->next = (d)->next;                                                                        \
    (d)->next->prev = (s)->prev;                                                                        \
    (s)->prev = (d)->prev;                                                                              \
    (d)->prev->next = (s);                                                                              \
    nsk_queue_init(d)

#define nsk_queue_insert_before(n1, n2) nsk_queue_insert_tail(n1, n2)
#define nsk_queue_insert_after(n1, n2) nsk_queue_insert_head(n1, n2)

#define nsk_queue_data(ptr, type, member) (type *)((char *)ptr - offsetof(type, member))

#endif // !_QUEUE_H_