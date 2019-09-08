#pragma once

#include "page.c"

// Assigns a page by virtual address to a physical address, using the specified root directory or NULL for default
void mem_page_assign(void* virt, void* phys, void* root, unsigned char flags);

// Removes a page by virtual address
void mem_page_remove(void* virt, void* root);

// Finds a free frame, allocates it, increments the specified page addr and returns the virtual addr
void* mem_page_request(size_t* counter, void* root, unsigned char flags);