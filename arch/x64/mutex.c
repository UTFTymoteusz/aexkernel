#include "aex/time.h"

#include <stdbool.h>

#include "aex/mutex.h"

void mutex_acquire(mutex_t* mutex) {
    while (!__sync_bool_compare_and_swap(mutex, 0, 1))
        asm volatile("pause");
}

void mutex_acquire_yield(mutex_t* mutex) {
    while (!__sync_bool_compare_and_swap(mutex, 0, 1)) {
        asm volatile("pause");
        yield();
    }
}

void mutex_release(mutex_t* mutex) {
    *mutex = 0;
}