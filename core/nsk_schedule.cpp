
#include "nsk_schedule.h"
#include "nsk_alloc.h"
#include "nsk_coroutine.h"
#include "nsk_timer_wheel.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>

struct nsk_pollset_t {
    int nevents;
#if !defined(__APPLE__) && !defined(__FreeBSD__)
    struct epoll_event *events;
#else
    struct kevent *eventlist;
#endif
};

typedef int (*nsk_co_handler_t)(void *);

struct nsk_fired_event_t {
    struct epoll_event ev;
    nsk_co_handler_t handler;
    int fd;
    void *arg;
    short int revents;
};

struct nsk_co_schedule_s {
    int poller_fd;
    int is_done;
    nsk_pollset_t pollset;
    nsk_fired_event_t *fired_events;
    nsk_timer_wheel_t *timer_collecter;
    nsk_sleep_pfn_t before_sleep_pfn;
    nsk_sleep_pfn_t after_sleep_pfn;
};

static int
co_epoll_wait(int epfd, struct nsk_pollset_t *pollset, int maxevents, int timeout) {
    return epoll_wait(epfd, pollset->events, maxevents, timeout);
}

static int
co_epoll_ctl(int epfd, int op, int fd, struct epoll_event *ev) {
    return epoll_ctl(epfd, op, fd, ev);
}

static int
co_epoll_create(int size) {
    return epoll_create(size);
}

static uint32_t
PollEvent2Epoll(short events) {
    uint32_t e = 0;
    if (events & POLLIN)
        e |= EPOLLIN;
    if (events & POLLOUT)
        e |= EPOLLOUT;
    if (events & POLLHUP)
        e |= EPOLLHUP;
    if (events & POLLERR)
        e |= EPOLLERR;
    if (events & POLLRDNORM)
        e |= EPOLLRDNORM;
    if (events & POLLWRNORM)
        e |= EPOLLWRNORM;
    return e;
}

static short
EpollEvent2Poll(uint32_t events) {
    short e = 0;
    if (events & EPOLLIN)
        e |= POLLIN;
    if (events & EPOLLOUT)
        e |= POLLOUT;
    if (events & EPOLLHUP)
        e |= POLLHUP;
    if (events & EPOLLERR)
        e |= POLLERR;
    if (events & EPOLLRDNORM)
        e |= POLLRDNORM;
    if (events & EPOLLWRNORM)
        e |= POLLWRNORM;
    return e;
}

nsk_co_schedule_t *
nsk_schedule_create(int nevents) {
    int size;
    nsk_co_schedule_t *schedule;

    size = nsk_align(sizeof(nsk_co_schedule_t), NSK_ALGINMENT);
    size += nsk_align(nevents * sizeof(struct epoll_event), NSK_ALGINMENT);
    size += nevents * sizeof(nsk_fired_event_t);
    schedule = (nsk_co_schedule_t *)nsk_memalign(NSK_ALGINMENT, size);
    schedule->pollset.nevents = nevents;

    schedule->pollset.events =
        (struct epoll_event *)((char *)schedule + nsk_align(sizeof(nsk_co_schedule_t), NSK_ALGINMENT));

    schedule->fired_events =
        (nsk_fired_event_t *)((char *)schedule->pollset.events +
                              nsk_align(nevents * sizeof(struct epoll_event), NSK_ALGINMENT));

    schedule->poller_fd = co_epoll_create(1024);

    if (schedule->poller_fd) {
        return NULL;
    }

    return schedule;
}

int
nsk_schedule_destory(nsk_co_schedule_t *schedule) {
    int err = 0;

    if (schedule->poller_fd) {
        err = close(schedule->poller_fd);
    }

    nsk_pfree(schedule);
    return err;
}

int
process_event(void *co) {
    nsk_coroutine_resume((nsk_coroutine_t *)co);
    return 0;
}

int
nsk_co_poll(
    nsk_co_schedule_t *schedule, struct pollfd fds[], nfds_t nfds, int timeout, poll_pfn_t poll_pfn) {
    nsk_fired_event_t *fired_event;
    nsk_coroutine_t *co;
    nsk_timer_event_t *event;
    struct epoll_event ev;
    int ret;

    if (timeout == 0) {
        return poll_pfn(fds, nfds, timeout);
    }

    if (timeout < 0) {
        timeout = -1;
    }

    co = nsk_get_current_thread_co();
    for (nfds_t n = 0; n < nfds; n++) {
        fired_event = schedule->fired_events + fds[n].fd;
        fired_event->fd = fds[n].fd;
        fired_event = schedule->fired_events + fds[n].fd;
        fired_event->arg = co;
        fired_event->handler = (nsk_co_handler_t)process_event;
        fired_event->ev.data.ptr = (void *)fired_event;
        fired_event->ev.events = PollEvent2Epoll(fds[n].events);
        if (co_epoll_ctl(schedule->poller_fd, EPOLL_CTL_ADD, fired_event->fd, &fired_event->ev) < 0) {
            printf("CO_ERR: co_epoll_ctl\n");
            return poll_pfn(fds, nfds, timeout);
        }
    }

    event = nsk_timer_wheel_add_timer(schedule->timer_collecter, timeout, process_event, (void *)co);
    if (!event) {
        printf("CO_ERR: AddTimeout ret");
        errno = EINVAL;
        ret = -1;
        return poll_pfn(fds, nfds, timeout);
    } else {
        nsk_coroutine_yield_ct();
        ret = 1;
    }

    if (event->has_time_out) {
        errno = ETIMEDOUT;
        ret = -1;
    }

    nsk_timer_wheel_cancel_timer(schedule->timer_collecter, &event);
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLONESHOT;

    for (nfds_t n = 0; n < nfds; n++) {
        fired_event = schedule->fired_events + fds[n].fd;
        co_epoll_ctl(schedule->poller_fd, EPOLL_CTL_DEL, fds[n].fd, &ev);
        fds[n].revents = EpollEvent2Poll(fired_event->revents);
    }

    return ret;
}

static int
nsk_schedule_is_done(nsk_co_schedule_t *schedule) {
    return schedule->is_done;
}

int
nsk_schedule_loop(nsk_co_schedule_t *schedule) {
    int err = 0;
    nsk_fired_event_t *fired_event;
    nsk_pollset_t *pollset = &schedule->pollset;
    nsk_timer_wheel_t *T = schedule->timer_collecter;
    int ready_events, i;
    const int events = schedule->pollset.nevents;
    const int every_wait_time = nsk_timer_wheel_get_wait_time_ms(T);

    while (!nsk_schedule_is_done(schedule)) {
        ready_events = co_epoll_wait(schedule->poller_fd, pollset, events, every_wait_time);
        for (i = 0; i < ready_events; i++) {
            fired_event = (nsk_fired_event_t *)pollset->events[i].data.ptr;
            fired_event->revents = pollset->events[i].events;
            fired_event->handler(fired_event->arg);
        }
        nsk_timer_wheel_dispatch(T);
    }

    return err;
}
