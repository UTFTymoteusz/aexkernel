#include "proc/task.h"

#include "aex/time.h"
#include "aex/mutex.h"

extern task_t* task_current;

void sleep(long delay) {
    if (!checkinterrupts())
        kpanic("sleep() attempt when interrupts are disabled");

    if (delay == -1) {
        task_remove(task_current);
        task_switch_full();
    }
    mutex_acquire(&(task_current->access));
    nointerrupts();

    task_put_to_sleep(task_current, delay);

    mutex_release(&(task_current->access));
    interrupts();
    
    task_switch_full();
}