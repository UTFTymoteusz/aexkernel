#include "proc/task.h"

#include "aex/time.h"

extern task_t* task_current;

void sleep(long delay) {
    if (delay == -1) {
        task_remove(task_current);
        task_switch_full();
    }
    task_current->status = TASK_STATUS_SLEEPING;
    task_current->resume_after = task_ticks + (delay / (1000 / CPU_TIMER_HZ));
    task_current->pass = true;

    task_switch_full();
}