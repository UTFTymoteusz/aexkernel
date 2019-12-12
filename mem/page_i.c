//#include "aex/dev/cpu.h"
#include "aex/proc/proc.h"

#include "aex/mem.h"
#include "page.h"

void* syscall_pgalloc(size_t bytes) {
    return kpalloc(kptopg(bytes), process_current->ptracker, 0x07);
}

void syscall_pgfree(void* page, size_t bytes) {
    kpfree(page, kptopg(bytes), process_current->ptracker);
}