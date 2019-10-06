#pragma once

#include "page.c"
#include "page_indepedent.c"

#include "aex/kmem.h"

void mempg_init();

// Assigns a page by virtual address to a physical address, using the specified root directory or NULL for default
void mempg_assign(void* virt, void* phys, void* root, unsigned char flags);

// Removes a page by virtual address
void mempg_remove(void* virt, void* root);

void* mempg_alloc(size_t amount, size_t* counter, void* root, unsigned char flags);
void* mempg_calloc(size_t amount, size_t* counter, void* root, unsigned char flags);

void* mempg_mapto(size_t amount, size_t* counter, void* phys_ptr, void* root, unsigned char flags);

void* mempg_paddrof(void* virt, void* root);

size_t mempg_to_pages(size_t bytes);