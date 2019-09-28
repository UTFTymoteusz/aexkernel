#pragma once

#include "pool.c"

void mempo_init();

mem_pool* mempo_create(uint32_t size);
void* mempo_alloc(uint32_t size);
void mempo_unalloc(void* space);

void mempo_enum_blocks(mem_pool* pool);
void mempo_cleanup(mem_pool* pool);