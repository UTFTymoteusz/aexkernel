#pragma once

//#include "aex/dev/cpu.h"
#include "pagetrk.h"

#define MEM_PAGE_MASK ~(CPU_PAGE_SIZE - 1)
#define MEM_PAGE_ENTRY_SIZE 16

page_tracker_t kernel_pgtrk;

void mempg_init();
void mempg_init2();

// Assigns a page by virtual address to a physical address.
void mempg_assign(void* virt, void* phys, page_tracker_t* tracker, uint8_t flags);

// Removes a page by virtual address.
void mempg_remove(void* virt, page_tracker_t* tracker);

void* mempg_mapvto(size_t amount, void* virt_ptr, void* phys_ptr, page_tracker_t* tracker, uint8_t flags);

// Creates a paging root directory. Returns the virtual address of the associated paging directory.
void* mempg_create_user_root(size_t* virt_addr);
// Disposes of a paging root directory.
void mempg_dispose_user_root(size_t virt_addr);

// Sets the current paging root to the trackers, useful in IO workers
void mempg_set_pagedir(page_tracker_t* tracker);

void* syscall_pgalloc(size_t bytes);
void  syscall_pgfree(void* page, size_t bytes);