#include "proc/task.h"

void yield() {
    task_switch_full();
}