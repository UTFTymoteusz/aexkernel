#include "aex/proc/task.h"

#include "aex/rwmutex.h"

bool rwmutex_acquire_read(rwmutex_t* rwmutex) {
    if (task_current == NULL)
        return true;

    while (rwmutex->write > 0) {
        if (rwmutex->removed)
            return false;

        yield();
    }
    task_use(task_current);
    __sync_add_and_fetch(&(rwmutex->write), 1);

    return true;
}

bool rwmutex_acquire_write(rwmutex_t* rwmutex) {
    if (task_current == NULL)
        return true;
        
    task_use(task_current);
    while (!__sync_bool_compare_and_swap(&(rwmutex->write), 0, 1)) {
        task_release(task_current);
        if (rwmutex->removed)
            return false;

        task_use(task_current);
    }

    while (rwmutex->read > 0)
        yield();

    return true;
}


void rwmutex_release_read(rwmutex_t* rwmutex) {
    __sync_sub_and_fetch(&(rwmutex->read), 1);
    task_release(task_current);
}

void rwmutex_release_write(rwmutex_t* rwmutex) {
    __sync_sub_and_fetch(&(rwmutex->write), 1);
    task_release(task_current);
}


void rwmutex_signal_remove(rwmutex_t* rwmutex) {
    rwmutex->removed = true;
}