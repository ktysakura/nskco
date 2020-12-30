#if !defined(_NSK_COROUTINE_INNER_)
#define _NSK_COROUTINE_INNER_

#include "nsk_coroutine.h"
#include "nsk_coctx.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define BIT(x) (1 << (x))

extern "C" {
extern void
coctx_swap(nsk_coroutine_ctx_t *, nsk_coroutine_ctx_t *) asm("coctx_swap");
};

struct nsk_coroutine_spec_s {
    void *value;
};

struct nsk_stack_mem_s {
    size_t stack_size;
    char * stack_buffer;
    char * stack_bp;
    char * stack_sp;
};

struct nsk_share_stack_s {
    size_t            stack_size;
    uint32_t          capacity;
    uint32_t          pos;
    nsk_stack_mem_t **stack_array;
};

struct nsk_coroutine_env_s {
    nsk_coroutine_t *call_stack[128];
    int16_t          call_stack_depth;
    nsk_coroutine_t *pending_co;
    nsk_coroutine_t *occupy_co;
};

struct nsk_coroutine_s {
    nsk_coroutine_env_t *env;
    nsk_stack_mem_t *    stack_mem;
    nsk_coroutine_ctx_t  ctx;
    nsk_coroutine_pfn_t  pfn;
    void *               arg;
    char *               stack_bp;
    char *               stack_sp;
    size_t               stack_size;

    unsigned char enable_shared_mem;
    unsigned char enable_sys_hook;
    unsigned char is_main;
    unsigned char save[1];

    nsk_coroutine_status_e status;
    unsigned char *        save_buffer;
    size_t                 save_size;

    nsk_coroutine_spec_t specs[1024];
};

static void *
nsk_coroutine_callback(nsk_coroutine_t *co, void *d) {
    int c;
    int e;
    printf("coroutine stack:%lx, %lx, %lx, %lx\n", (uintptr_t)co, (uintptr_t)d, (uintptr_t)&c,
           (uintptr_t)&e);
    if (co->pfn) {
        co->status = NSK_COROUTINE_STATUS_BUSY;
        co->pfn(co->arg);
    }
    co->status = NSK_COROUTINE_STATUS_EXITED;
	nsk_coroutine_yield_env(co->env);
}

static nsk_stack_mem_t *
nsk_alloc_stack_mem(size_t stack_size) {
    nsk_stack_mem_t *stack_mem;

    posix_memalign((void **)&stack_mem, NSK_ALGINMENT_SIZE, sizeof(nsk_stack_mem_t) + stack_size);
    stack_mem->stack_size = stack_size;
    stack_mem->stack_buffer = (char *)(stack_mem + 1);
    stack_mem->stack_sp = stack_mem->stack_buffer;
    stack_mem->stack_bp = stack_mem->stack_buffer + stack_size;
    printf("alignment 16 stack_buffer(%ld), stack_sp(%ld), stack_bp(%ld)\n",
           (uintptr_t)stack_mem->stack_buffer, (uintptr_t)stack_mem->stack_sp,
           (uintptr_t)stack_mem->stack_bp);
    return stack_mem;
}

static nsk_stack_mem_t *
nsk_get_mem_from_share_stack(nsk_share_stack_t *share_stack) {
    if (!share_stack) {
        return NULL;
    }

    return share_stack->stack_array[share_stack->pos++ % share_stack->capacity];
}

nsk_coroutine_t *
nsk_create_env(nsk_coroutine_env_t *       env,
               const nsk_coroutine_attr_t *attr,
               nsk_coroutine_pfn_t         routine,
               void *                      arg) {
    nsk_coroutine_attr_t at;
    nsk_coroutine_t *    co;

    if (attr) {
        memcpy(&at, attr, sizeof(at));
    }

    if (at.stack_size <= 0) {
        at.stack_size = kDefaultCoStackSize;
    } else if (at.stack_size > kDefaultCoStackLimitSize) {
        at.stack_size = kDefaultCoStackLimitSize;
    }

    if (at.stack_size & 0xFFF) {
        at.stack_size &= (~0xFFF);
        at.stack_size += 0x1000;
    }

    posix_memalign((void **)&co, NSK_ALGINMENT_SIZE, sizeof(nsk_coroutine_t));

    co->env = env;
    co->arg = arg;
    co->pfn = routine;

    if (at.share_memory) {
        co->stack_mem = nsk_get_mem_from_share_stack(at.share_memory);
        at.stack_size = at.share_memory->stack_size;
    } else {
        co->stack_mem = nsk_alloc_stack_mem(at.stack_size);
    }

    co->stack_sp = co->stack_mem->stack_sp;
    co->stack_bp = co->stack_mem->stack_bp;
    co->stack_size = at.stack_size;

    nsk_coctx_init(&co->ctx);
    co->ctx.ss_sp = co->stack_sp;
    co->ctx.ss_size = co->stack_size;

    co->enable_shared_mem = at.share_memory ? 1 : 0;
    co->enable_sys_hook = 0;

    co->status = NSK_COROUTINE_STATUS_NEW;
    co->save_buffer = NULL;
    co->save_size = 0;

    return co;
}

static nsk_coroutine_env_t *g_coroutine_env_per_thread = NULL;

static void
nsk_coroutine_init_current_thread_env() {
    nsk_coroutine_env_t *env;
    nsk_coroutine_t *    co_self;

    posix_memalign((void **)&g_coroutine_env_per_thread, NSK_ALGINMENT_SIZE,
                   sizeof(nsk_coroutine_env_t));

    env = g_coroutine_env_per_thread;

    co_self = nsk_create_env(env, NULL, NULL, NULL);
    co_self->is_main = 1;

    env->call_stack[env->call_stack_depth++] = co_self;
    env->occupy_co = NULL;
    env->pending_co = NULL;
}

static nsk_coroutine_env_t *
nsk_coroutine_get_current_thread_env() {
    return g_coroutine_env_per_thread;
}

int
nsk_coroutine_create(nsk_coroutine_t **          co,
                     const nsk_coroutine_attr_t *attr,
                     void *(*start_routine)(void *),
                     void *arg) {

    if (!g_coroutine_env_per_thread) {
        nsk_coroutine_init_current_thread_env();
    }

    *co = nsk_create_env(g_coroutine_env_per_thread, attr, start_routine, arg);

    return 0;
}

void
nsk_coroutine_resume(nsk_coroutine_t *co) {
    nsk_coroutine_env_t *env = co->env;
    nsk_coroutine_t *    cur_co = env->call_stack[env->call_stack_depth - 1];

    if (co->status == NSK_COROUTINE_STATUS_NEW) {
        nsk_coctx_make(&co->ctx, (nsk_coctx_pfn_t)nsk_coroutine_callback, co, 0);
        co->status = NSK_COROUTINE_STATUS_READY;
    }

    env->call_stack[env->call_stack_depth++] = co;
    coctx_swap(&cur_co->ctx, &co->ctx);

    if (co->status & BIT(NSK_COROUTINE_STATUS_EXITED)) {
        if (co->status & BIT(NSK_COROUTINE_STATUS_DETACH)) {
            // printf("nty_coroutine_resume --> \n");
            // nty_coroutine_free(co);
        }
    }
    return;
}

void
nsk_coroutine_yield_env(nsk_coroutine_env_t *env) {
    nsk_coroutine_t *last, *curr;

    last = env->call_stack[env->call_stack_depth - 2];
    curr = env->call_stack[env->call_stack_depth - 1];
    env->call_stack_depth--;

    coctx_swap(&curr->ctx, &last->ctx);
}

void
nsk_coroutine_yield(nsk_coroutine_t *co) {
    nsk_coroutine_yield_env(co->env);
}

void
nsk_coroutine_yield_ct() {
    nsk_coroutine_yield_env(nsk_coroutine_get_current_thread_env());
}

#endif
