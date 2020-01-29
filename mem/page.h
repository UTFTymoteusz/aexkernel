#pragma once

//#include "aex/dev/cpu.h"
#include "pagetrk.h"

#define MEM_PAGE_MASK ~0xFFF
#define MEM_PAGE_ENTRY_SIZE 16

page_root_t kernel_pgtrk;

void mempg_init();
void mempg_init2();

/*
// Assigns a page by virtual address to a physical address.
void mempg_assign(void* virt, phys_addr phys, page_root_t* proot, uint8_t flags);

// Removes a page by virtual address.
void mempg_remove(void* virt, page_root_t* proot);

void* mempg_mapvto(size_t amount, void* virt_ptr, phys_addr phys_ptr, page_root_t* proot, uint8_t flags);
*/

// Creates a paging directory structure. Returns the physical address of the associated paging directory root.
phys_addr mempg_create_user_paging_struct();
// Disposes of a paging directory structure.
void mempg_dispose_user_paging_struct(phys_addr addr);

// Sets the current paging root to the trackers, useful in IO workers
void mempg_set_pagedir(page_root_t* proot);

void* syscall_pgalloc(size_t bytes);
void  syscall_pgfree(void* page, size_t bytes);