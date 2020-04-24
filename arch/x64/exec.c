#include "aex/string.h"

#include "aex/proc/exec.h"

#include <stdarg.h>

size_t args_count(char* args[]) {
    int i = 0;
    while (args[i++] != NULL)
        ;
    
    return i;
}

size_t args_memlen(char* args[]) {
    int i = 0;
    int len = 0;
    while (args[i++] != NULL) {
        if (args[i] == NULL)
            continue;
            
        len += strlen(args[i]) + 1 + 8;
    }
    return len;
}

extern void* entry_caller;

void setup_entry_caller(void* addr, size_t proc_addr, char* args[]) {
    memcpy(addr, &entry_caller, 32);

    size_t offset = 32;
    int i = 0;
    int len;

    offset += args_count(args) * sizeof(size_t);
    do {
        if (args[i] == NULL) {
            *((size_t*) (addr + 32 + 8 * i)) = (size_t) NULL;
            continue;
        }
        len = strlen(args[i]) + 1;

        memcpy(addr + offset, args[i], len);
        *((size_t*) (addr + 32 + 8 * i)) = proc_addr + offset;

        offset += len;
    } while (args[i++] != NULL);
}

void init_entry_caller(thread_t* thread, void* entry, size_t init_code_addr, int arg_count) {
    thread->context.rax = (uint64_t) entry;
    thread->context.rdi = arg_count - 1;
    thread->context.rsi = init_code_addr + 32;
}

void set_arguments(UNUSED thread_t* thread, UNUSED_SOFAR int argc, ...) {
    kpanic("set_arguments()");

    /*va_list args;
    va_start(args, argc);

    task->context->rdi = va_arg(args, long);
    task->context->rsi = va_arg(args, long);
    task->context->rdx = va_arg(args, long);
    task->context->rcx = va_arg(args, long);
    task->context->r8  = va_arg(args, long);
    task->context->r9  = va_arg(args, long);

    va_end(args);*/
}

void set_varguments(thread_t* thread, UNUSED_SOFAR int argc, va_list args) {
    thread->context.rdi = va_arg(args, long);
    thread->context.rsi = va_arg(args, long);
    thread->context.rdx = va_arg(args, long);
    thread->context.rcx = va_arg(args, long);
    thread->context.r8  = va_arg(args, long);
    thread->context.r9  = va_arg(args, long);
}