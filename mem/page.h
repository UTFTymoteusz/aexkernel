#pragma once

#include "page.c"

// Assigns a page by virtual address to a physical address, using the specified root directory or NULL for default
void mempg_assign(void* virt, void* phys, void* root, unsigned char flags);

// Removes a page by virtual address
void mempg_remove(void* virt, void* root);

void* mempg_next(size_t amount, size_t* counter, void* root, unsigned char flags);
void* mempg_nextc(size_t amount, size_t* counter, void* root, unsigned char flags);

void* mempg_mapto(size_t amount, size_t* counter, void* phys_ptr, void* root, unsigned char flags);