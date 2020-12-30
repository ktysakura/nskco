#ifndef _NSK_COCTX_
#define _NSK_COCTX_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef void *(*nsk_coctx_pfn_t)(void *, void *);
typedef struct nsk_coroutine_ctx_s nsk_coroutine_ctx_t;

struct nsk_coroutine_ctx_s {
    void * regs[14];
    char * ss_sp;
    size_t ss_size;
};

int
nsk_coctx_init(nsk_coroutine_ctx_t *ctx);

int
nsk_coctx_make(nsk_coroutine_ctx_t *ctx, nsk_coctx_pfn_t pfn, const void *d, const void *s);

#endif