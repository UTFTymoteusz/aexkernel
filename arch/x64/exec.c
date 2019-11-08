#include "proc/exec.h"

#include <string.h>

extern void* entry_caller;

void setup_entry_caller(void* addr) {
    memcpy(addr, &entry_caller, CPU_ENTRY_CALLER_SIZE);
}

void init_entry_caller(task_descriptor_t* task, void* entry) {
    task->context->rax = (uint64_t) entry;
}