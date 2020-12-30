#if !defined(__NSK_COROUTINE_H_)
#define __NSK_COROUTINE_H_
#include <string.h>

typedef struct nsk_coroutine_s      nsk_coroutine_t;
typedef struct nsk_coroutine_attr_s nsk_coroutine_attr_t;
typedef struct nsk_stack_mem_s      nsk_stack_mem_t;
typedef struct nsk_share_stack_s    nsk_share_stack_t;
typedef struct nsk_coroutine_env_s  nsk_coroutine_env_t;
typedef struct nsk_coroutine_spec_s nsk_coroutine_spec_t;
typedef struct nsk_coroutine_ctx_s  nsk_coroutine_ctx_t;
typedef void *(*nsk_coroutine_pfn_t)(void *);

#define kDefaultCoStackSize (128 * 1024)
#define kDefaultCoStackLimitSize (kDefaultCoStackSize * 1024)
#define NSK_ALGINMENT_SIZE 16

struct nsk_coroutine_attr_s {
    size_t             stack_size;
    nsk_share_stack_t *share_memory;
    nsk_coroutine_attr_s() {
        stack_size = kDefaultCoStackSize;
        share_memory = NULL;
    }

} __attribute__((packed));

typedef enum {
    NSK_COROUTINE_STATUS_WAIT_READ,
    NSK_COROUTINE_STATUS_WAIT_WRITE,
    NSK_COROUTINE_STATUS_NEW,
    NSK_COROUTINE_STATUS_READY,
    NSK_COROUTINE_STATUS_EXITED,
    NSK_COROUTINE_STATUS_BUSY,
    NSK_COROUTINE_STATUS_SLEEPING,
    NSK_COROUTINE_STATUS_EXPIRED,
    NSK_COROUTINE_STATUS_FDEOF,
    NSK_COROUTINE_STATUS_DETACH,
    NSK_COROUTINE_STATUS_CANCELLED,
    NSK_COROUTINE_STATUS_PENDING_RUNCOMPUTE,
    NSK_COROUTINE_STATUS_RUNCOMPUTE,
    NSK_COROUTINE_STATUS_WAIT_IO_READ,
    NSK_COROUTINE_STATUS_WAIT_IO_WRITE,
    NSK_COROUTINE_STATUS_WAIT_MULTI
} nsk_coroutine_status_e;

int
nsk_coroutine_create(nsk_coroutine_t **          co,
                     const nsk_coroutine_attr_t *attr,
                     void *(*start_routine)(void *),
                     void *arg);

void
nsk_coroutine_resume(nsk_coroutine_t *co);

void
nsk_coroutine_yield_ct();

void
nsk_coroutine_yield(nsk_coroutine_t *co);

void
nsk_coroutine_yield_env(nsk_coroutine_env_t *env);
#endif // DEBUG
