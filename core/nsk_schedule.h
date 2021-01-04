#ifndef _NSK_CO_POLL_H
#define _NSK_CO_POLL_H

#include <poll.h>
#include <unistd.h>
typedef struct nsk_co_schedule_s nsk_co_schedule_t;
typedef void (*nsk_sleep_pfn_t)(nsk_co_schedule_t *);
typedef int (*poll_pfn_t)(struct pollfd[], nfds_t, int);

nsk_co_schedule_t *
nsk_schedule_create(int nevents);

int
nsk_schedule_destory(nsk_co_schedule_t *schedule);

int
nsk_schedule_loop(nsk_co_schedule_t *schedule);

nsk_co_schedule_t *
nsk_get_schedule_ct();
#endif
