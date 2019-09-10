#pragma once

#include "aex/kmem/kfree.c"
#include "aex/kmem/kmalloc.c"

// Allocates kernel memory
void* kmalloc(uint32_t size);

// Frees kernel memory
void kfree(void* block);