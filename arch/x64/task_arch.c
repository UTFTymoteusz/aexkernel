#include "aex/proc/task.h"

#include "cpu_int.h"

extern struct tss* cpu_tss;

void task_set_kernel_stack(thread_t* thread) {
    cpu_tss->rsp0 = (uint64_t) thread->kernel_stack;
}