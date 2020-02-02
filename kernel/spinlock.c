#include "aex/time.h"
#include "aex/sys.h"

#include "aex/proc/proc.h"
#include "aex/proc/task.h"

#include <stdbool.h>

#include "aex/spinlock.h"

void spinlock_acquire(spinlock_t* spinlock) {
    volatile int t = 0;
    while (!__sync_bool_compare_and_swap(&(spinlock->val), 0, 1)) {
        asm volatile("pause");

        t++;
        if (t > 1000000) {
            if (spinlock->name != NULL)
                printk("spinlock '%s' fail\n", spinlock->name);

            if (!checkinterrupts())
                printk(PRINTK_WARN "spinlock_acquire() without interrupts set");
            
            kpanic("spinlock_acquire stuck for too long");
        }
    }
}

void spinlock_release(spinlock_t* spinlock) {
    spinlock->val = 0;
}


bool spinlock_try(spinlock_t* spinlock) {
    return __sync_bool_compare_and_swap(&(spinlock->val), 0, 1);
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