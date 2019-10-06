#pragma once

#include <stdint.h>

// Allocates kernel memory
void* kmalloc(uint32_t size);

// Frees kernel memory
void kfree(void* block);