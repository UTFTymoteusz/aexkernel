#pragma once

//#include "aex/dev/cpu.h"

#define MEM_PAGE_MASK ~0xFFF
#define MEM_PAGE_ENTRY_SIZE 16

void mempg_init();
void mempg_init2();

void* syscall_pgalloc(size_t bytes);
void  syscall_pgfree(void* page, size_t bytes);