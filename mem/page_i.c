#include "aex/mem.h"
#include "aex/string.h"

#include "aex/proc/proc.h"

#include "page.h"

void* syscall_pgalloc(size_t bytes) {
    void* pointer = kpalloc(kptopg(bytes), process_current->proot, PAGE_USER | PAGE_WRITE);
    memset(pointer, 0, kpfrompg(kptopg(bytes)));

    return pointer;
}

void syscall_pgfree(void* page, size_t bytes) {
    kpfree(page, kptopg(bytes), process_current->proot);
}