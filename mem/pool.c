#pragma once

#include "frame.h"
#include "page.h"

#define DEFAULT_POOL_SIZE 0x1000 * 256

struct mem_pool {
    //uint32_t frame_start;
    uint32_t frame_amount;

    struct mem_pool* next;

    uint32_t size;
    uint32_t free;

    struct mem_block* first_block;
};
typedef struct mem_pool mem_pool_t;

struct mem_block {
    size_t size;
    char free : 1;

    struct mem_pool* parent;
    struct mem_block* next;
};
typedef struct mem_block mem_block_t;

mem_pool_t* mem_pool0;

void mempo_cleanup(mem_pool_t* pool);

mem_pool_t* mempo_create(uint32_t size) {
    uint32_t real_size     = size + sizeof(mem_pool_t) + sizeof(mem_block_t);
    uint32_t needed_frames = mempg_to_pages(real_size);
    void* ptr = mempg_nextc(needed_frames, NULL, NULL, 0x03);

    mem_pool_t* pool = (mem_pool_t*) ptr;
    mem_block_t* block = (mem_block_t*) (((size_t)ptr) + sizeof(mem_pool_t));

    block->size = size;
    block->free = true;
    block->parent = pool;
    block->next   = NULL;

    pool->frame_amount = needed_frames;
    pool->next = NULL;
    pool->size = size + sizeof(mem_block_t);
    pool->free = size;
    pool->first_block = block;

    return pool;
}

void* mempo_alloc(uint32_t size) {
    mem_pool_t* pool = mem_pool0;
    mem_block_t* block = mem_pool0->first_block;

    size += (size % 2);

    uint32_t real_size = size + sizeof(mem_block_t);

    while (true) {
        if ((!(block->free) && block->next == NULL) || block == NULL) {
            if (pool->next == NULL)
                pool->next = mempo_create(size > DEFAULT_POOL_SIZE ? size : DEFAULT_POOL_SIZE);

            pool = pool->next;
            block = pool->first_block;

            continue;
        }
        if (!(block->free)) {
            block = block->next;
            continue;
        }
        if (block->size == size) {
            block->free = false;
            pool->free -= size;

            break;
        }
        if (pool->free < real_size) {
            if (pool->next == NULL)
                pool->next = mempo_create(size > DEFAULT_POOL_SIZE ? size : DEFAULT_POOL_SIZE);

            pool  = pool->next;
            block = pool->first_block;

            continue;
        }
        if (block->size < real_size) {
            block = block->next;
            continue;
        }
        pool->free -= real_size;

        mem_block_t* new_block = (mem_block_t*)(((size_t)block) + real_size);

        new_block->size   = block->size - real_size;
        new_block->free   = true;
        new_block->parent = block->parent;
        new_block->next   = block->next;

        block->size = size;
        block->free = false;
        block->next = new_block;

        break;
    }
    return (void*)(((size_t)block) + sizeof(mem_block_t));
}

void mempo_unalloc(void* space) {
    mem_block_t* block = (mem_block_t*)(((size_t)space) - sizeof(mem_block_t));
    block->free = true;

    block->parent->free += block->size;

    mempo_cleanup(block->parent);
}

void mempo_enum(mem_pool_t* pool) {
    mem_block_t* block = pool->first_block;

    while (block != NULL) {
        printf("Addr: %x Size: %i ", (size_t)block & 0xFFFFFFFFFFFF, block->size);
        printf(block->free ? "Free\n" : "Busy\n");

        block = block->next;
    }
}

void mempo_cleanup(mem_pool_t* pool) {
    mem_block_t* block = pool->first_block;
    mem_block_t* next_block;

    while (block != NULL) {
        next_block = block->next;

        if (!(block->free)) {
            block = next_block;
            continue;
        }
        if (next_block == NULL)
            break;

        if (next_block->free) {
            block->size += sizeof(mem_block_t) + next_block->size;
            block->next = next_block->next;

            pool->size += sizeof(mem_block_t);

            continue;
        }
        block = next_block;
    }
}

void mempo_init() {
	mem_pool0 = mempo_create(DEFAULT_POOL_SIZE);
	printf("Created initial %i KB kernel memory pool\n\n", DEFAULT_POOL_SIZE / 1000);
}