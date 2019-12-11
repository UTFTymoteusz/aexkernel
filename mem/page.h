#pragma once

#include "dev/cpu.h"
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

/*
 * Allocates an amount of pages. 
 * Returns the allocated virtual address.
 */
void* mempg_alloc(size_t amount, page_tracker_t* tracker, uint8_t flags);
/**
 * Allocates an physically contiguous amount of pages. 
 * Returns the allocated virtual address.
 */
void* mempg_calloc(size_t amount, page_tracker_t* tracker, uint8_t flags);
// Frees pages and unallocates the related frames, starting at the specified id.
void  mempg_free(void* virt, size_t amount, page_tracker_t* tracker);
// Unassociates frames, starting at the specified id.
void  mempg_unmap(void* virt, size_t amount, page_tracker_t* tracker);

// Maps the amount of pages to the specified physical address. Returns the allocated virtual address.
void* mempg_mapto(size_t amount, void* phys_ptr, page_tracker_t* tracker, uint8_t flags);
void* mempg_mapvto(size_t amount, void* virt_ptr, void* phys_ptr, page_tracker_t* tracker, uint8_t flags);

// Returns the physical address of a virtual address.
void* mempg_paddrof(void* virt, page_tracker_t* tracker);

// Returns the page index of a virtual address.
uint64_t mempg_indexof(void* virt, page_tracker_t* tracker);
// Returns the frame id of a virtual address, or 0xFFFFFFFF if it's an arbitrary mapping (e.g. from mempg_mapto()).
uint64_t mempg_frameof(void* virt, page_tracker_t* tracker);

// Creates a paging root directory. Returns the virtual address of the associated paging directory.
void* mempg_create_user_root(size_t* virt_addr);
// Disposes of a paging root directory.
void mempg_dispose_user_root(size_t virt_addr);

// Sets the current paging root to the trackers, useful in IO workers
void mempg_set_pagedir(page_tracker_t* tracker);

// Returns the number of pages required to fit the specified amount of bytes.
static inline size_t mempg_to_pages(size_t bytes) {
    return (bytes + (CPU_PAGE_SIZE - 1)) / CPU_PAGE_SIZE;
}

void* syscall_pgalloc(size_t bytes);
void  syscall_pgfree(void* page, size_t bytes);