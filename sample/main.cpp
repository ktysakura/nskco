#include "nsk_timer_wheel.h"
#include <cstdint>
#include <iostream>
#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdio.h>
#include <unistd.h>
#include <unistd.h>
#include <unordered_map>
using namespace std;

struct scheduler_t {
    nsk_queue_t defer_queue;
    nsk_queue_t ready_queue;
};

struct coroutine_t {
    int value;
    nsk_queue_node_t defer;
    nsk_queue_node_t ready;
};

void
test() {
}

void
test_link_list() {
    scheduler_t scheduler, scheduler1, spilt;
    coroutine_t s1, s2, s3, s4, s5, s6;
    s1.value = 10;
    s2.value = 20;
    s3.value = 30;
    s4.value = 40;
    s5.value = 50;
    s6.value = 60;
    nsk_queue_init(&scheduler.defer_queue);
    nsk_queue_init(&scheduler1.defer_queue);
    nsk_queue_init(&spilt.defer_queue);
    // nsk_queue_insert_tail(&scheduler.defer_queue, &s1.defer);
    // nsk_queue_insert_tail(&scheduler.defer_queue, &s2.defer);
    nsk_queue_insert_head(&scheduler.defer_queue, &s1.defer);
    nsk_queue_insert_head(&scheduler.defer_queue, &s2.defer);
    nsk_queue_insert_after(&s1.defer, &s3.defer);
    nsk_queue_insert_before(&s2.defer, &s4.defer);
    nsk_queue_remove(&s2.defer);
    nsk_queue_remove(&s1.defer);
    nsk_queue_remove(&s3.defer);
    nsk_queue_insert_tail(&scheduler1.defer_queue, &s5.defer);
    nsk_queue_insert_tail(&scheduler1.defer_queue, &s6.defer);
    nsk_queue_merge(&scheduler.defer_queue, &scheduler1.defer_queue);
    for (nsk_queue_node_t *node = nsk_queue_head(&scheduler.defer_queue);
         node != nsk_queue_sentinel(&scheduler.defer_queue); node = nsk_queue_next(node)) {
        coroutine_t *p = nsk_queue_data(node, coroutine_t, defer);
        cout << p->value << endl;
    }

    cout << std::endl;
    nsk_queue_spilt(&scheduler.defer_queue, &spilt.defer_queue, &s5.defer);
    for (nsk_queue_node_t *node = nsk_queue_head(&scheduler.defer_queue);
         node != nsk_queue_sentinel(&scheduler.defer_queue); node = nsk_queue_next(node)) {
        coroutine_t *p = nsk_queue_data(node, coroutine_t, defer);
        cout << p->value << endl;
    }

    cout << std::endl;
    for (nsk_queue_node_t *node = nsk_queue_head(&spilt.defer_queue);
         node != nsk_queue_sentinel(&spilt.defer_queue); node = nsk_queue_next(node)) {
        coroutine_t *p = nsk_queue_data(node, coroutine_t, defer);
        cout << p->value << endl;
    }
}

int
hello_world(void *node) {
    printf("hello world msec\n");
    return 0;
}

void
test_time_wheel() {
    nsk_timer_wheel_t *T = nsk_timer_wheel_create(1, 1, 1);
    nsk_timer_event_t *node;

    node = nsk_timer_wheel_add_timer(T, 60001, hello_world, NULL);
    node = nsk_timer_wheel_add_timer(T, 2000, hello_world, NULL);
    node = nsk_timer_wheel_add_timer(T, 1000, hello_world, NULL);
    node = nsk_timer_wheel_add_timer(T, 2000, hello_world, NULL);
    node = nsk_timer_wheel_add_timer(T, 1000, hello_world, NULL);
    node = nsk_timer_wheel_add_timer(T, 2000, hello_world, NULL);
    node = nsk_timer_wheel_add_timer(T, 1000, hello_world, NULL);
    node = nsk_timer_wheel_add_timer(T, 4000, hello_world, NULL);
    node = nsk_timer_wheel_add_timer(T, 1500, hello_world, NULL);
    node = nsk_timer_wheel_add_timer(T, 3000, hello_world, NULL);
     nsk_timer_wheel_cancel_timer(T, &node);
    nsk_timer_wheel_queue_t timeout;
    nsk_queue_init(&timeout);
    for (;;) {
    }
    nsk_timer_wheel_destroy(T);
}

int
main() {
    int a = 0xff00;
    int b = 0xff44;
    if ((a | ((1 << 8) - 1)) == (b | ((1 << 8) - 1))) {
        printf("eq\n");
    } else {
        printf("neq\n");
    }
    // test_link_list();
    test_time_wheel();
    printf("hello world\n");
    // stRoutineArgs_t args[10];
    // for (int i = 0; i < 10; i++)
    // {
    // 	args[i].routine_id = i;
    // 	co_create(&args[i].co, NULL, RoutineFunc, (void*)&args[i]);
    // 	co_resume(args[i].co);
    // }
    // 	co_eventloop(co_get_epoll_ct(), NULL, NULL);
    return 0;
}
