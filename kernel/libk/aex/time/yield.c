#include "proc/task.h"

void yield() {
    task_current->status = TASK_STATUS_YIELDED;
    task_switch_full();
}