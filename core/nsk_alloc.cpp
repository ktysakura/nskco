#include "nsk_alloc.h"

// 基本的页大小,ngx_pagesize = getpagesize()
// 通常是4k
uint64_t nsk_pagesize;

// 页大小的左移数
// 左移数,4k即2^12,值12
uint64_t nsk_pagesize_shift;

// 宏定义为64
// 然后由ngx_cpuinfo（ngx_cpuinfo.c）来探测
uint64_t nsk_cacheline_size;
int nsk_is_os_init = 0;
void *
nsk_alloc(size_t size) {
    return malloc(size);
}

void *
nsk_calloc(size_t size) {
    void *p;

    p = nsk_alloc(size);
    if (p) {
        memset(p, 0, size);
    }
    return p;
}

void *
nsk_memalign(int alignment, size_t size) {
    void *p;
    int err;

    err = posix_memalign(&p, alignment, size);
    if (err) {
        return NULL;
    }

    return p;
}

void
nsk_os_info_init() {

    if (nsk_is_os_init) {
        return;
    }

    nsk_is_os_init = 1;
    nsk_pagesize = getpagesize();

    for (uint32_t n = nsk_pagesize; n >>= 1; nsk_pagesize_shift++) {
        /*void*/
    }

    nsk_cacheline_size = 64;
}