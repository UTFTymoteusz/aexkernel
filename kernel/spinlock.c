#include "aex/kernel.h"
#include "aex/proc/task.h"
#include "aex/sys/cpu.h"

#include <stdbool.h>

#include "aex/spinlock.h"

void spinlock_acquire(spinlock_t* spinlock) {
    task_reshed_disable();

    volatile int t = 0;
    while (!__sync_bool_compare_and_swap(&(spinlock->val), 0, 1)) {
        asm volatile("pause");

        t++;
        if (t > 1000000) {
            if (spinlock->name != NULL)
                printk("spinlock '%s' fail\n", spinlock->name);

            kpanic("spinlock_acquire stuck for too long");
        }
    }
}

void spinlock_release(spinlock_t* spinlock) {
    spinlock->val = 0;
    task_reshed_enable();
}


bool spinlock_try(spinlock_t* spinlock) {
    task_reshed_disable();

    bool res = __sync_bool_compare_and_swap(&(spinlock->val), 0, 1);
    if (!res)
        task_reshed_enable();

    return res;
}

void spinlock_wait(spinlock_t* spinlock) {
    int t = 0;
    while (spinlock->val) {
        asm volatile("pause");
        t++;
        if (t > 1000000)
            kpanic("spinlock_wait stuck for too long");
    }
}