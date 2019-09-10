#pragma once

void sleep(size_t delay) {

    task_current->sreg_a = delay / (1000 / CPU_TIMER_HZ);
    task_current->pass = true;

    task_remove(task_current, TASK_QUEUE_RUNNABLE);
    task_insert(task_current, TASK_QUEUE_SLEEPING);

    task_switch_full();
}