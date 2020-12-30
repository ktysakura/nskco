#include "nsk_coctx.h"
#include <stdio.h>
enum { kRDI = 7, kRSI = 8, kRETAddr = 9, kRSP = 13 };

int
nsk_coctx_init(nsk_coroutine_ctx_t *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    return 0;
}

int
nsk_coctx_make(nsk_coroutine_ctx_t *ctx, nsk_coctx_pfn_t pfn, const void *d, const void *s) {
    void **ret_addr;
    char * sp;

    sp = (ctx->ss_sp + ctx->ss_size) - sizeof(void *);
    sp = (char *)((uintptr_t)(sp) & -16LL); // low 16 bits 0x0000
    ret_addr = (void **)sp;
    *ret_addr = (void *)pfn;
    ctx->regs[kRDI] = (void *)d;
    ctx->regs[kRSI] = (void *)s;
    ctx->regs[kRETAddr] = (void *)pfn;
    ctx->regs[kRSP] = (void *)sp;
    printf("nsk_coctx_make : %lx, %lx,%lx\n", (uintptr_t)d, (uintptr_t)s, (uintptr_t)sp);
    return 0;
}
