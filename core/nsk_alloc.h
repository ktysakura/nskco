#ifndef _NSK_ALLOC_CPP_
#define _NSK_ALLOC_CPP_
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NSK_ALGINMENT 16

#define nsk_align(x, d) ((d) & ((d)-1)) ? (x) : (((x) + ((d)-1)) & ~((d)-1))

#define nsk_align_ptr(p, d)                                                                             \
    ((d) & ((d)-1)) ? (p) : (char *)(((uintptr_t)(p) + ((uintptr_t)d - 1)) & ~((uintptr_t)d - 1))

void *
nsk_alloc(size_t size);

void *
nsk_calloc(size_t size);

void *
nsk_memalign(int alignment, size_t size);

#define nsk_pfree(p)                                                                                    \
    if (p) {                                                                                            \
        free(p);                                                                                        \
        p = NULL;                                                                                       \
    }

void
nsk_os_info_init();
// 基本的页大小,ngx_pagesize = getpagesize()
// 通常是4k
extern uint64_t nsk_pagesize;

// 页大小的左移数
// 左移数,4k即2^12,值12
extern uint64_t nsk_pagesize_shift;

// 宏定义为64
// 然后由ngx_cpuinfo（ngx_cpuinfo.c）来探测
extern uint64_t nsk_cacheline_size;
#endif