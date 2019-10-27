#include "dev/cpu.h"
#include "proc/proc.h"

#include "page.h"

void* syscall_pgalloc(size_t bytes) {
    nointerrupts();
    void* ret = mempg_alloc(mempg_to_pages(bytes), process_current->ptracker, 0x07);
    interrupts();
    return ret;
}

void syscall_pgfree(void* page, size_t bytes) {
    nointerrupts();
    mempg_free(mempg_indexof(page, process_current->ptracker), mempg_to_pages(bytes), process_current->ptracker);
    interrupts();
}