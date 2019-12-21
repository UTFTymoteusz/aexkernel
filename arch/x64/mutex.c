#include "aex/time.h"
#include "aex/sys.h"

#include "aex/dev/cpu.h"

#include <stdbool.h>

#include "aex/mutex.h"

void mutex_acquire(mutex_t* mutex) {
    while (!__sync_bool_compare_and_swap(mutex, 0, 1)) {
        asm volatile("pause");
        if (!checkinterrupts())
            kpanic("mutex_acquire() without interrupts set");
    }
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

bool mutex_try(mutex_t* mutex) {
    return __sync_bool_compare_and_swap(mutex, 0, 1);
}

void mutex_wait(mutex_t* mutex) {
    while (*mutex) {
        asm volatile("pause");
        if (!checkinterrupts())
            kpanic("mutex_wait() without interrupts set");
    }
}

void mutex_wait_yield(mutex_t* mutex) {
    while (*mutex) {
        asm volatile("pause");
        yield();
    }
}