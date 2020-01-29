#include "aex/mutex.h"

#include "aex/proc/task.h"

#include "aex/time.h"

extern task_t* task_current;

void sleep(long delay) {
    if (delay == -1) {
        task_remove(task_current);
        task_switch_full();
    }
    mutex_acquire(&(task_current->access));
    nointerrupts();

    task_put_to_sleep(task_current, delay);

    interrupts();
    mutex_release(&(task_current->access));
    
    task_switch_full();
}

void yield() {
    task_switch_full();
}

double get_ms_passed() {
    return task_ms_per_tick * task_ticks;
}