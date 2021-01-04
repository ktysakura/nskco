#include "nsk_alloc.h"
#include "nsk_coroutine.h"
#include "nsk_schedule.h"
#include <dlfcn.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <resolv.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    int user_flag;
    struct sockaddr_in dest;
    int domain;
    struct timeval recv_timeout;
    struct timeval send_timeout;
    struct timeval connect_timeout;
} rpchook_t;

static rpchook_t *g_rpchook_socket_fd[kDefaultMaxFD] = {0};

#define DECLARE_SYSTEM_FUNC(name)                                                                       \
    static name##_pfn_t g_sys_##name##_func = (name##_pfn_t)dlsym(RTLD_NEXT, #name);

#define HOOK_SYS_FUNC(name)                                                                             \
    if (!g_sys_##name##_func) {                                                                         \
        g_sys_##name##_func = (name##_pfn_t)dlsym(RTLD_NEXT, #name);                                    \
    }

#if EAGAIN != EWOULDBLOCK
#define _IO_NOT_READY_ERROR ((errno == EAGAIN) || (errno == EWOULDBLOCK))
#else
#define _IO_NOT_READY_ERROR (errno == EAGAIN)
#endif

#define POLL_TIMEOUT(t) (((t).tv_sec * 1000) + ((t).tv_usec / 1000))

typedef int (*socket_pfn_t)(int, int, int);
typedef int (*connect_pfn_t)(int, struct sockaddr *, socklen_t);
typedef int (*accept_pfn_t)(int, struct sockaddr *, socklen_t *);
typedef int (*close_pfn_t)(int);

typedef ssize_t (*read_pfn_t)(int, void *, size_t);
typedef ssize_t (*write_pfn_t)(int, const void *, size_t);

typedef ssize_t (*readv_pfn_t)(int, const struct iovec *, int);
typedef ssize_t (*writev_pfn_t)(int, const struct iovec *, int);

typedef ssize_t (*sendto_pfn_t)(int, const void *, size_t, int, const struct sockaddr *, socklen_t);
typedef ssize_t (*recvfrom_pfn_t)(int, void *, size_t, int, struct sockaddr *, socklen_t *);

typedef ssize_t (*send_pfn_t)(int, const void *, size_t, int);
typedef ssize_t (*recv_pfn_t)(int, void *, size_t, int);
typedef ssize_t (*sendfile_pfn_t)(int, int, off_t *, size_t);

typedef int (*setsockopt_pfn_t)(int, int, int, const void *, socklen_t);

typedef int (*poll_pfn_t)(struct pollfd[], nfds_t, int);
typedef int (*__poll_pfn_t)(struct pollfd[], nfds_t, int);

typedef int (*fcntl_pfn_t)(int, int, ...);
typedef struct tm *(*localtime_r_pfn_t)(const time_t *, struct tm *);

typedef void *(*pthread_getspecific_pfn_t)(pthread_key_t);
typedef int (*pthread_setspecific_pfn_t)(pthread_key_t, const void *);

typedef int (*setenv_pfn_t)(const char *, const char *, int);
typedef int (*unsetenv_pfn_t)(const char *);
typedef char *(*getenv_pfn_t)(const char *);
typedef hostent *(*gethostbyname_pfn_t)(const char *);
typedef res_state (*__res_state_pfn_t)();

typedef int (*gethostbyname_r_pfn_t)(const char *__restrict,
                                     struct hostent *__restrict,
                                     char *__restrict,
                                     size_t,
                                     struct hostent **__restrict,
                                     int *__restrict);

DECLARE_SYSTEM_FUNC(socket)
DECLARE_SYSTEM_FUNC(connect)
DECLARE_SYSTEM_FUNC(accept)
DECLARE_SYSTEM_FUNC(close)
DECLARE_SYSTEM_FUNC(read)
DECLARE_SYSTEM_FUNC(write)
DECLARE_SYSTEM_FUNC(readv)
DECLARE_SYSTEM_FUNC(writev)
DECLARE_SYSTEM_FUNC(sendto)
DECLARE_SYSTEM_FUNC(recvfrom)
DECLARE_SYSTEM_FUNC(send)
DECLARE_SYSTEM_FUNC(recv)
DECLARE_SYSTEM_FUNC(sendfile)
DECLARE_SYSTEM_FUNC(setsockopt)
DECLARE_SYSTEM_FUNC(poll)
DECLARE_SYSTEM_FUNC(__poll)
DECLARE_SYSTEM_FUNC(fcntl)
DECLARE_SYSTEM_FUNC(setenv)
DECLARE_SYSTEM_FUNC(unsetenv)
DECLARE_SYSTEM_FUNC(gethostbyname)
DECLARE_SYSTEM_FUNC(__res_state)
DECLARE_SYSTEM_FUNC(gethostbyname_r)

static inline rpchook_t *
alloc_by_fd(int fd) {
    if (fd > -1 && fd < (int)(sizeof(g_rpchook_socket_fd) / sizeof(g_rpchook_socket_fd[0]))) {
        rpchook_t *rpchook = (rpchook_t *)nsk_calloc(sizeof(rpchook_t));

        rpchook->recv_timeout.tv_sec = 1;
        rpchook->send_timeout.tv_sec = 1;
        rpchook->connect_timeout.tv_sec = 75;

        g_rpchook_socket_fd[fd] = rpchook;
        return rpchook;
    }
    return NULL;
}

static inline void
free_by_fd(int fd) {
    if (fd > -1 && fd < (int)(sizeof(g_rpchook_socket_fd) / sizeof(g_rpchook_socket_fd[0]))) {
        nsk_pfree(g_rpchook_socket_fd[fd]);
    }
}

static inline rpchook_t *
get_by_fd(int fd) {
    if (fd > -1 && fd < (int)(sizeof(g_rpchook_socket_fd) / sizeof(g_rpchook_socket_fd[0]))) {
        return g_rpchook_socket_fd[fd];
    }
    return NULL;
}

int
socket(int domain, int type, int protocol) {
    HOOK_SYS_FUNC(socket)

    rpchook_t *rpchook;

    if (!(nsk_co_is_enable_sys_hook())) {
        return g_sys_socket_func(domain, type, protocol);
    }

    int fd = g_sys_socket_func(domain, type, protocol);
    if (!rpchook) {
        return fd;
    }

    rpchook = alloc_by_fd(fd);
    rpchook->domain = domain;

    fcntl(fd, F_SETFL, g_sys_fcntl_func(fd, F_GETFL, 0));

    return fd;
}

int
connect(int fd, struct sockaddr *address, socklen_t addrlen) {
    HOOK_SYS_FUNC(connect)

    int ret;
    rpchook_t *rpchook;

    if (!(nsk_co_is_enable_sys_hook())) {
        return g_sys_connect_func(fd, address, addrlen);
    }

eintr:
    ret = g_sys_connect_func(fd, address, addrlen);

    if (ret < 0 && (errno == EINTR || errno == EWOULDBLOCK)) {
        goto eintr;
    }

    rpchook = get_by_fd(fd);

    if (!rpchook) {
        return ret;
    }

    if (sizeof(rpchook->dest) >= addrlen) {
        memcpy(&rpchook->dest, address, addrlen);
    }

    if (O_NONBLOCK | rpchook->user_flag) {
        return ret;
    }

    if (ret >= 0) {
        return ret;
    }

    if (errno != EINPROGRESS && errno != EADDRINUSE) {
        return ret;
    }

    int timeout;
    int pollret;
    struct pollfd pfd = {fd, (POLLOUT | POLLHUP | POLLERR), 0};

    timeout = POLL_TIMEOUT(rpchook->connect_timeout);

    for (int n = 0; n < 3; n++) {

        pollret = poll(&pfd, 1, timeout);
        if (pollret == 1) {
            break;
        }
    }

    if (pfd.revents & POLLOUT) {
        int err;
        socklen_t socklen = sizeof(err);

        ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &socklen);
        if (ret < 0) {
            return ret;
        } else if (err) {
            errno = err;
            return -1;
        }
        errno = 0;
        return ret;
    }

    return ret;
}

int
accept(int fd, struct sockaddr *addr, socklen_t *len) {
    HOOK_SYS_FUNC(accept)

    rpchook_t *rpchook;

    if (!(nsk_co_is_enable_sys_hook())) {
        return g_sys_accept_func(fd, addr, len);
    }

    rpchook = get_by_fd(fd);
    if (!rpchook || O_NONBLOCK & rpchook->user_flag) {
        return g_sys_accept_func(fd, addr, len);
    }

    return 0;
}

ssize_t
read(int fd, void *buf, size_t len) {
    HOOK_SYS_FUNC(read)

    rpchook_t *rpchook;

    if (!(nsk_co_is_enable_sys_hook())) {
        return g_sys_read_func(fd, buf, len);
    }

    rpchook = get_by_fd(fd);

    if (!rpchook || !(O_NONBLOCK & rpchook->user_flag)) {
        return g_sys_read_func(fd, buf, len);
    }

    ssize_t n;
    int timeout;
    pollfd pfd = {fd, POLLIN | POLLERR | POLLHUP, 0};

    timeout = POLL_TIMEOUT(rpchook->recv_timeout);

    while ((n = g_sys_read_func(fd, buf, len)) < 0) {
        if (errno == EINTR) {
            continue;
        }

        if (!_IO_NOT_READY_ERROR) {
            return -1;
        }

        if (poll(&pfd, 1, timeout) < 0) {
            return -1;
        }
    }

    return n;
}

ssize_t
write(int fd, const void *buf, size_t len) {
    HOOK_SYS_FUNC(write)

    rpchook_t *rpchook;

    if (!(nsk_co_is_enable_sys_hook())) {
        return g_sys_write_func(fd, buf, len);
    }

    rpchook = get_by_fd(fd);
    if (!rpchook || O_NONBLOCK & rpchook->user_flag) {
        return g_sys_write_func(fd, buf, len);
    }

    int timeout;
    ssize_t n;
    size_t nwritten = 0;
    pollfd pfd = {fd, POLLOUT | POLLERR | POLLHUP, 0};

    timeout = POLL_TIMEOUT(rpchook->send_timeout);

    while (nwritten < len) {
        n = g_sys_write_func(fd, buf, len - nwritten);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            if (!_IO_NOT_READY_ERROR) {
                return -1;
            }
        }

        nwritten += n;
        if (nwritten == len) {
            break;
        }

        if (poll(&pfd, 1, timeout) < 0) {
            break;
        }
    }

    return nwritten ? nwritten : -1;
}

ssize_t
readv(int fd, const struct iovec *iov, int iovcnt) {
    HOOK_SYS_FUNC(readv)

    rpchook_t *rpchook;

    if (!(nsk_co_is_enable_sys_hook())) {
        return g_sys_readv_func(fd, iov, iovcnt);
    }

    rpchook = get_by_fd(fd);
    if (!rpchook) {
        return g_sys_readv_func(fd, iov, iovcnt);
    }

    if (!rpchook && O_NONBLOCK & rpchook->user_flag) {
        return g_sys_readv_func(fd, iov, iovcnt);
    }
    int timeout;
    ssize_t n, nread;
    pollfd pfd = {fd, POLLIN | POLLHUP | POLLERR, 0};

    timeout = POLL_TIMEOUT(rpchook->recv_timeout);

    do {
        if (iovcnt > 1) {
            n = g_sys_readv_func(fd, iov, iovcnt);
        } else {
            n = g_sys_read_func(fd, iov[0].iov_base, iov[0].iov_len);
        }

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            if (!_IO_NOT_READY_ERROR) {
                return -1;
            }
        }

        if (n >= 0) {
            break;
        }

        if (poll(&pfd, 1, timeout) < 0) {
            n = -1;
            break;
        }
    } while (1);

    return n;
}

ssize_t
writev(int fd, const struct iovec *iov, int iovcnt) {
    HOOK_SYS_FUNC(writev)

    rpchook_t *rpchook;

    if (!(nsk_co_is_enable_sys_hook())) {
        return g_sys_writev_func(fd, iov, iovcnt);
    }

    rpchook = get_by_fd(fd);
    if (!rpchook || O_NONBLOCK & rpchook->user_flag) {
        return g_sys_writev_func(fd, iov, iovcnt);
    }
    struct iovec *piov;
    ssize_t n, nleft, nbyte, nwritten, pos, idx;
    pollfd pfd = {fd, POLLOUT | POLLHUP | POLLERR, 0};
    int timeout;

    n = nleft = nbyte = nwritten = pos = 0;
    timeout = POLL_TIMEOUT(rpchook->send_timeout);

    for (int i = 0; i < iovcnt; i++) {
        nbyte += iov[i].iov_len;
    }

    nleft = nbyte;

    piov = (iovec *)iov;
    while (nleft > 0) {
        if (iovcnt > 1) {
            n = g_sys_writev_func(fd, piov, iovcnt);
        } else {
            n = g_sys_write_func(fd, piov[0].iov_base, piov[0].iov_len);
        }

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            if (!_IO_NOT_READY_ERROR) {
                return -1;
            }
        }

        piov[0].iov_base = ((char *)piov[0].iov_base - pos);
        piov[0].iov_len = piov[0].iov_len + pos;

        if (n == nleft) {
            break;
        }

        nleft -= n;
        n = nbyte - nleft;

        for (idx = 0; n > iov[idx].iov_len; idx++) {
            n -= iov[idx].iov_len;
        }

        pos = n;
        piov = (iovec *)&iov[idx];

        piov[0].iov_base = ((char *)piov[0].iov_base + pos);
        piov[0].iov_len = piov[0].iov_len - pos;

        if (poll(&pfd, 1, timeout) < 0) {
            n = -1;
            break;
        }
    }

    return n;
}

ssize_t
recv(int fd, void *buf, size_t len, int flags) {
    HOOK_SYS_FUNC(recv)

    rpchook_t *rpchook;

    if (!(nsk_co_is_enable_sys_hook())) {
        return g_sys_recv_func(fd, buf, len, flags);
    }

    rpchook = get_by_fd(fd);

    if (!rpchook || !(O_NONBLOCK & rpchook->user_flag)) {
        return g_sys_recv_func(fd, buf, len, flags);
    }

    ssize_t n;
    int timeout;
    pollfd pfd = {fd, POLLIN | POLLERR | POLLHUP, 0};

    timeout = POLL_TIMEOUT(rpchook->recv_timeout);

    while ((n = g_sys_recv_func(fd, buf, len, flags)) < 0) {
        if (errno == EINTR) {
            continue;
        }

        if (!_IO_NOT_READY_ERROR) {
            return -1;
        }

        if (poll(&pfd, 1, timeout) < 0) {
            return -1;
        }
    }

    return n;
}

ssize_t
send(int fd, const void *buf, size_t len, int flags) {
    HOOK_SYS_FUNC(send)

    rpchook_t *rpchook;

    if (!(nsk_co_is_enable_sys_hook())) {
        return g_sys_send_func(fd, buf, len, flags);
    }

    rpchook = get_by_fd(fd);
    if (!rpchook || O_NONBLOCK & rpchook->user_flag) {
        return g_sys_send_func(fd, buf, len, flags);
    }

    int timeout;
    ssize_t n;
    size_t nwritten = 0;
    pollfd pfd = {fd, POLLOUT | POLLERR | POLLHUP, 0};

    timeout = POLL_TIMEOUT(rpchook->send_timeout);

    while (nwritten < len) {
        n = g_sys_send_func(fd, buf, len - nwritten, flags);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            if (!_IO_NOT_READY_ERROR) {
                return -1;
            }
        }

        nwritten += n;
        if (nwritten == len) {
            break;
        }

        if (poll(&pfd, 1, timeout) < 0) {
            return -1;
        }
    }

    return nwritten ? nwritten : -1;
}

ssize_t
recvfrom(int fd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    HOOK_SYS_FUNC(recvfrom)

    rpchook_t *rpchook;

    if (!(nsk_co_is_enable_sys_hook())) {
        return g_sys_recvfrom_func(fd, buf, len, flags, src_addr, addrlen);
    }

    rpchook = get_by_fd(fd);
    if (!rpchook || O_NONBLOCK & rpchook->user_flag) {
        return g_sys_recvfrom_func(fd, buf, len, flags, src_addr, addrlen);
    }

    ssize_t n;
    int timeout;
    pollfd pfd = {fd, POLLIN | POLLERR | POLLHUP, 0};

    timeout = POLL_TIMEOUT(rpchook->recv_timeout);

    while ((n = g_sys_recvfrom_func(fd, buf, len, flags, src_addr, addrlen)) < 0) {
        if (errno == EINTR) {
            continue;
        }

        if (!_IO_NOT_READY_ERROR) {
            return -1;
        }

        if (poll(&pfd, 1, timeout) < 0) {
            return -1;
        }
    }

    return n;
}
ssize_t
sendto(int fd,
       const void *buf,
       size_t len,
       int flags,
       const struct sockaddr *dest_addr,
       socklen_t addrlen) {
    HOOK_SYS_FUNC(sendto)

    rpchook_t *rpchook;

    if (!(nsk_co_is_enable_sys_hook())) {
        return g_sys_sendto_func(fd, buf, len, flags, dest_addr, addrlen);
    }

    rpchook = get_by_fd(fd);
    if (!rpchook || O_NONBLOCK & rpchook->user_flag) {
        return g_sys_sendto_func(fd, buf, len, flags, dest_addr, addrlen);
    }

    ssize_t n;
    int timeout;
    pollfd pfd = {fd, POLLOUT | POLLERR | POLLHUP, 0};

    timeout = POLL_TIMEOUT(rpchook->recv_timeout);

    while ((n = g_sys_sendto_func(fd, buf, len, flags, dest_addr, addrlen)) < 0) {
        if (errno == EINTR) {
            continue;
        }

        if (!_IO_NOT_READY_ERROR) {
            return -1;
        }

        if (poll(&pfd, 1, timeout) < 0) {
            return -1;
        }
    }

    return n;
}

ssize_t
sendfile(int out_fd, int in_fd, off_t *offset, size_t count) {
    HOOK_SYS_FUNC(sendfile)

    rpchook_t *rpchook;

    if (!(nsk_co_is_enable_sys_hook())) {
        return g_sys_sendfile_func(out_fd, in_fd, offset, count);
    }

    rpchook = get_by_fd(out_fd);
    if (!rpchook || O_NONBLOCK & rpchook->user_flag) {
        return g_sys_sendfile_func(out_fd, in_fd, offset, count);
    }

    ssize_t timeout, nleft, nsent, nbyte, n;
    pollfd pfd = {out_fd, POLLOUT | POLLERR | POLLHUP, 0};

    nbyte = nleft = count;
    timeout = POLL_TIMEOUT(rpchook->send_timeout);

    while (nleft > 0) {
        n = g_sys_sendfile_func(out_fd, in_fd, offset + nsent, nleft);
        if (errno == EINTR) {
            continue;
        }

        if (!_IO_NOT_READY_ERROR) {
            return -1;
        }

        nsent += n;
        nleft -= n;

        if (nsent == nbyte) {
            break;
        }

        if (poll(&pfd, 1, timeout) < 0) {
            return -1;
        }
    }

    return nsent;
}

int
close(int fd) {
    HOOK_SYS_FUNC(close)

    if (!(nsk_co_is_enable_sys_hook())) {
        return g_sys_close_func(fd);
    }

    free_by_fd(fd);
    return g_sys_close_func(fd);
}

int
fcntl(int fd, int cmd, ... /* arg */) {
    HOOK_SYS_FUNC(fcntl);

    if (fd < 0) {
        return __LINE__;
    }

    va_list arg_list;
    va_start(arg_list, cmd);

    int ret = -1;
    rpchook_t *lp = get_by_fd(fd);

    switch (cmd) {

    case F_DUPFD: {
        int param = va_arg(arg_list, int);
        ret = g_sys_fcntl_func(fd, cmd, param);
        break;
    }

    case F_GETFD: {
        ret = g_sys_fcntl_func(fd, cmd);
        break;
    }

    case F_SETFD: {
        int param = va_arg(arg_list, int);
        ret = g_sys_fcntl_func(fd, cmd, param);
        break;
    }

    case F_GETFL: {
        ret = g_sys_fcntl_func(fd, cmd);
        if (lp && !(lp->user_flag & O_NONBLOCK)) {
            ret = ret & (~O_NONBLOCK);
        }
        break;
    }

    case F_SETFL: {
        int param = va_arg(arg_list, int);
        int flag = param;
        if (nsk_co_is_enable_sys_hook() && lp) {
            flag |= O_NONBLOCK; //设置非阻塞
        }
        ret = g_sys_fcntl_func(fd, cmd, flag);
        if (0 == ret && lp) {
            lp->user_flag = param;
        }
        break;
    }

    case F_GETOWN: {
        ret = g_sys_fcntl_func(fd, cmd);
        break;
    }

    case F_SETOWN: {
        int param = va_arg(arg_list, int);
        ret = g_sys_fcntl_func(fd, cmd, param);
        break;
    }

    case F_GETLK: {
        struct flock *param = va_arg(arg_list, struct flock *);
        ret = g_sys_fcntl_func(fd, cmd, param);
        break;
    }

    case F_SETLK: {
        struct flock *param = va_arg(arg_list, struct flock *);
        ret = g_sys_fcntl_func(fd, cmd, param);
        break;
    }

    case F_SETLKW: {
        struct flock *param = va_arg(arg_list, struct flock *);
        ret = g_sys_fcntl_func(fd, cmd, param);
        break;
    }
    }

    va_end(arg_list);

    return ret;
}

extern int
nsk_co_poll(
    nsk_co_schedule_t *schedule, struct pollfd fds[], nfds_t nfds, int timeout, poll_pfn_t poll_pfn);

int
poll(struct pollfd fds[], nfds_t nfds, int timeout) {
    HOOK_SYS_FUNC(poll)

    if (!nsk_co_is_enable_sys_hook() || timeout == 0) {
        return g_sys_poll_func(fds, nfds, timeout);
    }
    return nsk_co_poll(nsk_get_schedule_ct(), fds, nfds, timeout, g_sys_poll_func);
}
