#include "nsk_timer_wheel.h"
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

struct nsk_timer_wheel_s {
    uint16_t           near_slot_size;
    uint16_t           slot_size;
    nsk_queue_t *      near;
    nsk_queue_t **     slots;
    nsk_queue_t        timeout;
    uint16_t           level;
    uint32_t           time_scale;
    uint64_t           start_time_point;
    uint64_t           current_point;
    uint64_t           timer_tick;
    pthread_spinlock_t locker;
    uint8_t            thread_safe;
};

#define TIME_NEAR_SHIFT 8
#define TIME_NEAR (1 << TIME_NEAR_SHIFT)
#define TIME_LEVEL_SHIFT 6
#define TIME_LEVEL (1 << TIME_LEVEL_SHIFT)
#define TIME_NEAR_MASK (TIME_NEAR - 1)
#define TIME_LEVEL_MASK (TIME_LEVEL - 1)
#define TIMER_CACHE_ALIGNMENT 16
static const uint64_t kDefaultNearSlotSize = 60 * 1000;

uint64_t
gettime(int scale) {
    uint64_t t;
#if !defined(__APPLE__) || defined(AVAILABLE_MAC_OS_X_VERSION_10_12_AND_t)
    struct timespec ti;
    clock_gettime(CLOCK_MONOTONIC, &ti);
    t = (uint64_t)ti.tv_sec * scale;
    t += ti.tv_nsec / 1000000000 * scale;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    t = (uint64_t)tv.tv_sec * scale;
    t += tv.tv_usec / 1000000 * scale;
#endif
    return t;
}

nsk_timer_wheel_t *
nsk_timer_wheel_create(uint16_t level, uint16_t time_scale, uint8_t thread_safe) {
    nsk_timer_wheel_t *T;
    int                i, j;
    int                ret = 0;

    if (level != 1 && level != 5) {
        errno = EINVAL;
        return NULL;
    }

    if (time_scale > 1000) {
        errno = EINVAL;
        return NULL;
    }

    ret = posix_memalign((void **)&T, TIMER_CACHE_ALIGNMENT, sizeof(nsk_timer_wheel_t));
    if (ret != 0) {
        errno = ENOMEM;
        return NULL;
    }

    memset(T, 0, sizeof(nsk_timer_wheel_t));
    T->level = level;
    T->time_scale = 1000 / time_scale;
    T->thread_safe = thread_safe;
    T->near_slot_size = (level == 1 ? kDefaultNearSlotSize : TIME_NEAR);
    T->slot_size = TIME_LEVEL;
    T->timer_tick = 0;
    T->start_time_point = gettime(T->time_scale);
    T->current_point = T->start_time_point;

    if (!T->thread_safe) {
        pthread_spin_init(&T->locker, PTHREAD_PROCESS_SHARED);
    }

    nsk_queue_init(&T->timeout);

    ret = posix_memalign((void **)&T->near, TIMER_CACHE_ALIGNMENT,
                         sizeof(nsk_timer_wheel_queue_t) * T->near_slot_size);

    for (i = 0; i < T->near_slot_size; i++) {
        nsk_queue_init(&T->near[i]);
    }

    if (ret != 0) {
        return NULL;
    }

    if (T->level != 1) {
        ret = posix_memalign((void **)&T->slots, TIMER_CACHE_ALIGNMENT,
                             sizeof(nsk_timer_wheel_queue_t *) * (T->level - 1));
        if (ret != 0) {
            errno = ENOMEM;
            return NULL;
        }
    }

    for (i = 0; i < (T->level - 1); i++) {
        posix_memalign((void **)&T->slots[i], TIMER_CACHE_ALIGNMENT,
                       sizeof(nsk_timer_wheel_queue_t) * TIME_LEVEL);
    }

    for (i = 0; i < (T->level - 1); i++) {
        for (j = 0; j < TIME_LEVEL; j++) {
            nsk_queue_init(&T->slots[i][j]);
        }
    }

    return T;
}

int
nsk_timer_wheel_destroy(nsk_timer_wheel_t *T) {
    if (T == NULL) {
        return 0;
    }

    if (T->near) {
        free(T->near);
    }

    if (T->slots) {
        for (int i = 0; i < (T->level - 1); i++) {
            free(T->slots[i]);
        }
        free(T->slots);
    }

    free(T);
    return 0;
}

static void
nsk_timer_wheel_add_node(nsk_timer_wheel_t *T, nsk_timer_event_t *event) {
    uint64_t time = event->expired_time;
    event->ref++;
    //(0x(EF00|0xFF) == (0xEF10|0xFF)) ==> both in the near level
    if (T->level == 1 || (time | TIME_NEAR_MASK) == (T->timer_tick | TIME_NEAR_MASK)) {
        nsk_queue_insert_tail(&T->near[time % T->near_slot_size], &event->timer_node);
    } else {
        int      level;
        uint32_t mask = (TIME_NEAR << TIME_LEVEL_SHIFT);
        // ignore low bit, only compare high bit
        for (level = 0; level < T->level - 1; level++) {
            if ((time | (mask - 1)) == (T->timer_tick | (mask - 1))) {
                break;
            }
            mask <<= TIME_LEVEL_SHIFT;
        }

        event->level = level + 1;
        nsk_timer_event_node_t tmp_node = event->timer_node;

        nsk_queue_insert_tail(
            &T->slots[level][(time >> (TIME_NEAR_SHIFT + level * TIME_LEVEL_SHIFT)) & TIME_LEVEL_MASK],
            &event->timer_node);
    }
}

static nsk_timer_event_t *
nsk_timer_wheel_add_timer_inner(nsk_timer_wheel_t *T,
                                uint64_t           timeout,
                                timeout_handler_t  handler,
                                void *             arg) {
    nsk_timer_event_t *event = (nsk_timer_event_t *)calloc(sizeof(nsk_timer_event_t), 1);
    uint64_t           diff = timeout - T->timer_tick;

    if (T->level == 1 && diff >= T->near_slot_size) {
        printf("CO_ERR: nsk_timer_wheel_add_timer_inner line %d diff=%lu > %lu \n", __LINE__, diff,
               kDefaultNearSlotSize);
        diff = T->near_slot_size - 1;
    }

    event->expired_time = T->timer_tick + diff;
    event->handler = handler;
    event->arg = arg;
    event->has_time_out = 0;
    event->cancel = 0;
    event->ref = 0;
    if (event->expired_time <= 0) {
        event->handler(event);
        free(event);
        return NULL;
    }

    nsk_timer_wheel_add_node(T, event);

    return event;
}

nsk_timer_event_t *
nsk_timer_wheel_add_timer(nsk_timer_wheel_t *T, uint64_t timeout, timeout_handler_t handler, void *arg) {
    return nsk_timer_wheel_add_timer_inner(T, timeout / (1000.0 / T->time_scale), handler, arg);
}

int
nsk_timer_wheel_cancel_timer(nsk_timer_wheel_t *T, nsk_timer_event_t **node) {
    (*node)->cancel = 1;
    (*node)->has_time_out = true;
    (*node)->ref--;
    TIMER_LOCK(T);
    nsk_queue_remove(&(*node)->timer_node);
    TIMER_UNLOCK(T);
    free(*node);
    *node = NULL;
    return 0;
}

nsk_timer_event_t *
nsk_timer_wheel_get_timer(nsk_timer_event_node_t *node) {
    return nsk_queue_data(node, nsk_timer_event_t, timer_node);
}

int
nsk_timer_wheel_move_list(nsk_timer_wheel_t *T, uint16_t level, uint16_t slot_idx) {
    nsk_queue_t *      queue = &T->slots[level][slot_idx];
    nsk_queue_t *      node;
    nsk_timer_event_t *data;

    while (!nsk_queue_empty(queue)) {
        node = nsk_queue_head(queue);
        nsk_queue_remove(node);
        data = nsk_queue_data(node, nsk_timer_event_t, timer_node);
        nsk_timer_wheel_add_node(T, data);
    }

    return 0;
}

static void
nsk_timer_wheel_shift(nsk_timer_wheel_t *T) {
    int      mask = TIME_NEAR;
    uint64_t ct = ++T->timer_tick;

    if (T->level == 1) {
        return;
    }

    if (ct == 0) {
        nsk_timer_wheel_move_list(T, 3, 0);
    } else {
        uint32_t time = ct >> TIME_NEAR_SHIFT;
        uint16_t level = 0, slot = 0;

        while ((ct & (mask - 1)) == 0) {

            slot = time & TIME_LEVEL_MASK;

            if (slot != 0) {
                nsk_timer_wheel_move_list(T, level, slot);
            }

            mask <<= TIME_LEVEL_SHIFT;
            time >>= TIME_LEVEL_SHIFT;
            level++;
        }
    }
}

void
nsk_timer_wheel_dispatch_list(nsk_timer_wheel_t *T, nsk_timer_wheel_queue_t *active) {
    nsk_timer_event_node_t *node = nsk_queue_head(active);
    nsk_timer_event_t *     event;
    int                     next_delay;

    while (node != nsk_queue_sentinel(active)) {
        event = nsk_queue_data(node, nsk_timer_event_t, timer_node);
        node = nsk_queue_next(node);

        if (!event->has_time_out && event->cancel == 0) {
            next_delay = event->handler(event);
        }

        event->has_time_out = true;
        event->ref--;
        event->cancel = 1;

        if (event->ref == 0) {
            free(event);
        }
    }
}

void
nsk_timer_wheel_update_queue_inner(nsk_timer_wheel_t *T) {
    TIMER_LOCK(T);
    nsk_timer_wheel_queue_t *near;
    near = &T->near[T->timer_tick % T->near_slot_size];
    nsk_queue_merge(&T->timeout, near);
    nsk_timer_wheel_shift(T);
    near = &T->near[T->timer_tick % T->near_slot_size];
    nsk_queue_merge(&T->timeout, near);
    TIMER_UNLOCK(T);
}

void
nsk_timer_wheel_update_time(nsk_timer_wheel_t *T) {
    uint64_t cp = gettime(T->time_scale);

    if (cp != T->start_time_point) {
        uint64_t diff = cp - T->start_time_point;
        T->start_time_point = cp;
        T->current_point += diff;
        for (int i = 0; i < diff; i++) {
            nsk_timer_wheel_update_queue_inner(T);
        }
    }
}

int
nsk_timer_wheel_expect_timeout_queue(nsk_timer_wheel_t *T, nsk_queue_t *timeout) {
    nsk_timer_wheel_update_time(T);
    if (!nsk_queue_empty(&T->timeout)) {
        TIMER_LOCK(T);
        if (!nsk_queue_empty(&T->timeout)) {
            nsk_queue_move_head(&T->timeout, timeout);
        }
        TIMER_UNLOCK(T);
    }
    return 0;
}