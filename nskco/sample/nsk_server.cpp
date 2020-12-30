#include <iostream>
#include <nsk_coctx.h>
#include <nsk_coroutine.h>
#include <nsk_timer_wheel.h>
using namespace std;

void *
server_accept(void *) {
    return NULL;
}

void *
client_conn(void *) {
    printf("client\n");
    return NULL;
}

int
main() {
    nsk_coroutine_t *co1;
    nsk_coroutine_create(&co1, NULL, server_accept, NULL);
    nsk_coroutine_resume(co1);
    return 0;
}