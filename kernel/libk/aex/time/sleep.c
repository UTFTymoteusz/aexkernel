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

    task_current->status = TASK_STATUS_SLEEPING;
    task_current->resume_after = task_ticks + (delay / (1000 / CPU_TIMER_HZ));

    mutex_release(&(task_current->access));
    task_switch_full();
}