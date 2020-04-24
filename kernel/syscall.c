#include "aex/kernel.h"
#include "aex/rcode.h"

#include "aex/proc/task.h"

#include <stddef.h>

#include "aex/syscall.h"

void* syscalls[SYSCALL_AMOUNT];

void* syscall_wrapper(int id, void* a, void* b, void* c, 
                void* d, void* e) {
        
    if (id >= SYSCALL_AMOUNT) {
        printk(PRINTK_WARN "syscall: pid %i out of bounds (%i)\n", 
            CURRENT_PID, id);
        return (void*) -ENOSYS;
    }
    
    void* (*func)(void*, void*, void*, void*, void*) = syscalls[id];
    if (func == NULL) {
        printk(PRINTK_WARN "syscall: pid %i null (%i)\n", 
            CURRENT_PID, id);
        return (void*) -ENOSYS;
    }

    task_tuse(CURRENT_TID);
    void* ret = func(a, b, c, d, e);
    task_trelease(CURRENT_TID);
    return ret;
}