#include "aex/mem.h"
#include "aex/string.h"
#include "aex/syscall.h"
#include "aex/proc/task.h"

void* syscall_pgalloc(size_t bytes) {
    void* pointer = kpalloc(kptopg(bytes), task_prget_pmap(CURRENT_PID), 
                        PAGE_USER | PAGE_WRITE);
    memset(pointer, 0, kpfrompg(kptopg(bytes)));
    return pointer;
}

void syscall_pgfree(void* page, size_t bytes) {
    kpfree(page, kptopg(bytes), task_prget_pmap(CURRENT_PID));
}

void mem_syscall_init() {
    syscalls[SYSCALL_PGALLOC] = syscall_pgalloc;
    syscalls[SYSCALL_PGFREE]  = syscall_pgfree;
}