#pragma once

#include "pool.c"

struct mem_pool;
typedef struct mem_pool mem_pool_t;

void mempo_init();

mem_pool_t* mempo_create(uint32_t size);
void* mempo_alloc(uint32_t size);
void  mempo_unalloc(void* space);

void mempo_enum(mem_pool_t* pool);
void mempo_cleanup(mem_pool_t* pool);