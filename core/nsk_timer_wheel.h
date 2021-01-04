#ifndef TIME_WHELL_H
#define TIME_WHELL_H
#include "nsk_queue.h"
#include <pthread.h>
#include <stdint.h>
#define likely(x) __builtin_expect(!!x, 1)
#define unlikely(x) __builtin_expect(!!x, 0)

typedef struct nsk_timer_event_s nsk_timer_event_t;
typedef nsk_queue_node_t nsk_timer_event_node_t;
typedef nsk_queue_t nsk_timer_wheel_queue_t;
typedef struct nsk_timer_wheel_s nsk_timer_wheel_t;

typedef int (*timeout_handler_s)(void *);
typedef timeout_handler_s timeout_handler_t;

struct nsk_timer_event_s {
    uint64_t expired_time;
    timeout_handler_t handler;
    void *arg;
    nsk_timer_event_node_t timer_node;
    uint8_t has_time_out;
    uint8_t cancel;
    uint16_t ref;
    uint16_t level;
};

#define NSK_TIMER_LOCK(priv_data)                                                                       \
    if (likely(!priv_data->thread_safe)) {                                                              \
        pthread_spin_lock(&(priv_data->locker));                                                        \
    }

#define NSK_TIMER_UNLOCK(priv_data)                                                                     \
    if (likely(!priv_data->thread_safe)) {                                                              \
        pthread_spin_unlock(&(priv_data->locker));                                                      \
    }

nsk_timer_wheel_t *
nsk_timer_wheel_create(uint16_t level, uint16_t time_scale, uint8_t thread_safe);

int
nsk_timer_wheel_destroy(nsk_timer_wheel_t *T);

nsk_timer_event_t *
nsk_timer_wheel_add_timer(nsk_timer_wheel_t *T, uint64_t timeout, timeout_handler_t handler, void *arg);

int
nsk_timer_wheel_cancel_timer(nsk_timer_wheel_t *T, nsk_timer_event_t **node);

nsk_timer_event_t *
nsk_timer_wheel_get_timer(nsk_timer_event_node_t *node);

int
nsk_timer_wheel_get_wait_time_ms(nsk_timer_wheel_t *T);

void
nsk_timer_wheel_update_time(nsk_timer_wheel_t *T);

int
nsk_timer_wheel_dispatch(nsk_timer_wheel_t *T);

#endif