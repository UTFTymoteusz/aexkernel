#include "proc/exec.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

extern void* entry_caller;

void setup_entry_caller(void* addr) {
    memcpy(addr, &entry_caller, CPU_ENTRY_CALLER_SIZE);
}

void init_entry_caller(task_t* task, void* entry) {
    task->context->rax = (uint64_t) entry;
}

void set_arguments(task_t* task, ...) {
    va_list args;
    va_start(args, task);

    task->context->rdi = va_arg(args, long);
    task->context->rsi = va_arg(args, long);
    task->context->rdx = va_arg(args, long);
    task->context->rcx = va_arg(args, long);
    task->context->r8  = va_arg(args, long);
    task->context->r9  = va_arg(args, long);

    va_end(args);
}