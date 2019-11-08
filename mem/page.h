#pragma once

#include "dev/cpu.h"
#include "pagetrk.h"

#define MEM_PAGE_MASK ~(CPU_PAGE_SIZE - 1)
#define MEM_PAGE_ENTRY_SIZE 16

page_tracker_t kernel_pgtrk;

void mempg_init();
void mempg_init2();

// Assigns a page by virtual address to a physical address, using the specified root directory or NULL for default
void mempg_assign(void* virt, void* phys, page_tracker_t* tracker, uint8_t flags);

// Removes a page by virtual address
void mempg_remove(void* virt, page_tracker_t* tracker);

void* mempg_alloc(size_t amount, page_tracker_t* tracker, uint8_t flags);
void* mempg_calloc(size_t amount, page_tracker_t* tracker, uint8_t flags);
void  mempg_free(uint64_t id, size_t amount, page_tracker_t* tracker);
void  mempg_unassoc(uint64_t id, size_t amount, page_tracker_t* tracker);

void* mempg_mapto(size_t amount, void* phys_ptr, page_tracker_t* tracker, uint8_t flags);
void* mempg_mapvto(size_t amount, void* virt_ptr, void* phys_ptr, page_tracker_t* tracker, uint8_t flags);

void* mempg_paddrof(void* virt, page_tracker_t* tracker);

uint64_t mempg_indexof(void* virt, page_tracker_t* tracker);
uint64_t mempg_frameof(void* virt, page_tracker_t* tracker);

void* mempg_create_user_root(size_t* virt_addr);
void mempg_dispose_user_root(size_t virt_addr);

void mempg_set_pagedir(page_tracker_t* tracker);

static inline size_t mempg_to_pages(size_t bytes) {
    return (bytes + (CPU_PAGE_SIZE - 1)) / CPU_PAGE_SIZE;
}

void* syscall_pgalloc(size_t bytes);
void  syscall_pgfree(void* page, size_t bytes);