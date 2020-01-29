#include "aex/time.h"
#include "aex/sys.h"

#include "aex/dev/cpu.h"
#include "aex/proc/proc.h"
#include "aex/proc/task.h"

#include <stdbool.h>

#include "aex/mutex.h"

void mutex_acquire(mutex_t* mutex) {
    if (task_current == NULL)
        return;
        
    task_use(task_current);

    volatile int t = 0;
    while (!__sync_bool_compare_and_swap(&(mutex->val), 0, 1)) {
        if (!checkinterrupts())
            kpanic("mutex_acquire() without interrupts set");

        asm volatile("pause");
        task_release(task_current);

        t++;
        if (t > 1000000) {
            if (mutex->name != NULL)
                printk("mutex '%s' fail\n", mutex->name);
            
            kpanic("mutex_acquire stuck for too long");
        }
        task_use(task_current);
    }
    //if (mutex->name != NULL)
    //    printk("mutex '%s' acquired - %i\n", mutex->name, task_current->busy);
}

void mutex_acquire_yield(mutex_t* mutex) {
    if (task_current == NULL)
        return;

    task_use(task_current);

    volatile int t = 0;
    while (!__sync_bool_compare_and_swap(&(mutex->val), 0, 1)) {
        if (!checkinterrupts())
            kpanic("mutex_acquire_yield() without interrupts set");

        task_release(task_current);

        asm volatile("pause");
        yield();

        t++;
        if (t > 1000000) {
            if (mutex->name != NULL)
                printk("mutex '%s' fail\n", mutex->name);

            kpanic("mutex_acquire_yield stuck for too long");
        }
        task_use(task_current);
    }
    //if (mutex->name != NULL)
    //    printk("mutex '%s' acquired (yield) - %i\n", mutex->name, task_current->busy);
}

void mutex_acquire_raw(mutex_t* mutex) {
    int t = 0;
    while (!__sync_bool_compare_and_swap(&(mutex->val), 0, 1)) {
        asm volatile("pause");
        if (!checkinterrupts())
            kpanic("mutex_acquire() without interrupts set");

        t++;
        if (t > 1000000)
            kpanic("mutex_acquire_raw stuck for too long");
    }
}

void mutex_release(mutex_t* mutex) {
    mutex->val = 0;

    if (task_current == NULL)
        return;

    //if (mutex->name != NULL)
    //    printk("mutex '%s' released ", mutex->name, task_current->busy);

    task_release(task_current);

    //if (mutex->name != NULL)
    //    printk("- %i\n", task_current->busy);

    if (task_current->busy < 0) {
        //printk("AAAAAAA: 0x%08p ''''%s''''\n", mutex, mutex->name);
        kpanic("the goddamned busy is < 0");
    }
}

void mutex_release_raw(mutex_t* mutex) {
    mutex->val = 0;
}

bool mutex_try(mutex_t* mutex) {
    if (task_current == NULL)
        return true;

    task_use(task_current);

    bool ret = __sync_bool_compare_and_swap(&(mutex->val), 0, 1);
    if (ret == false)
        task_release(task_current);

    return ret;
}

bool mutex_try_raw(mutex_t* mutex) {
    return __sync_bool_compare_and_swap(&(mutex->val), 0, 1);
}

void mutex_wait(mutex_t* mutex) {
    while (mutex->val) {
        asm volatile("pause");
        if (!checkinterrupts())
            kpanic("mutex_wait() without interrupts set");
    }
}

void mutex_wait_yield(mutex_t* mutex) {
    while (mutex->val) {
        asm volatile("pause");
        yield();
    }
}