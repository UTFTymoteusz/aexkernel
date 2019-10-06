#pragma once

#include "dev/cpu.h"

#define MEM_PAGE_MASK ~(CPU_PAGE_SIZE - 1)
#define MEM_PAGE_ENTRY_SIZE 16

void mempg_init();

// Assigns a page by virtual address to a physical address, using the specified root directory or NULL for default
void mempg_assign(void* virt, void* phys, void* root, unsigned char flags);

// Removes a page by virtual address
void mempg_remove(void* virt, void* root);

void* mempg_alloc(size_t amount, size_t* counter, void* root, unsigned char flags);
void* mempg_calloc(size_t amount, size_t* counter, void* root, unsigned char flags);

void* mempg_mapto(size_t amount, size_t* counter, void* phys_ptr, void* root, unsigned char flags);

void* mempg_paddrof(void* virt, void* root);

inline size_t mempg_to_pages(size_t bytes) {
    return (bytes + (CPU_PAGE_SIZE - 1)) / CPU_PAGE_SIZE;
}