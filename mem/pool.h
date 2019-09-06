#pragma once

#include "mem/pool.c"

void mem_pool_init();

mem_pool* mem_pool_create(uint32_t size);
void* mem_pool_alloc(uint32_t size);
void mem_pool_unalloc(void* space);

void mem_pool_enum_blocks(mem_pool* pool);
void mem_pool_cleanup(mem_pool* pool);