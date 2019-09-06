#pragma once

#include "mem/frame.h"

#define DEFAULT_POOL_SIZE 0x1000 * 256

typedef struct mem_pool {
    uint32_t frame_start;
    uint32_t frame_amount;
    
    struct mem_pool* next;

    uint32_t size;
    uint32_t free;

    struct mem_block* first_block;
} mem_pool;

typedef struct mem_block {
    size_t size;
    char free : 1;

    struct mem_pool* parent;
    struct mem_block* next;
} mem_block;

mem_pool* mem_pool0;
//size_t isr_stack;

mem_pool* mem_pool_create(uint32_t size) {

    uint32_t real_size = size + sizeof(mem_pool) + sizeof(mem_block);
    uint32_t needed_frames = 0;

    for (uint32_t i = 0; i < real_size; i += MEM_FRAME_SIZE)
        ++needed_frames;

    uint32_t start_id = mem_frame_alloc_contiguous(needed_frames);
    void* ptr = mem_frame_get_ptr(start_id);

    mem_pool* pool = (mem_pool*) ptr;
    mem_block* block = (mem_block*) (((size_t)ptr) + sizeof(mem_pool));

    block->size = size;
    block->free = true;
    block->parent = pool;
    block->next = NULL;

    pool->frame_start = start_id;
    pool->frame_amount = needed_frames;
    pool->next = NULL;
    pool->size = size + sizeof(mem_block);
    pool->free = size;
    pool->first_block = block;

    return pool;
}
void* mem_pool_alloc(uint32_t size) {

    mem_pool* pool = mem_pool0;
    mem_block* block = mem_pool0->first_block;

    uint32_t real_size = size + sizeof(mem_block);

    while (true) {

        if ((!(block->free) && block->next == NULL) || block == NULL) {
            //printf("Reached end\n");

            if (pool->next == NULL)
                pool->next = mem_pool_create(size > DEFAULT_POOL_SIZE ? size : DEFAULT_POOL_SIZE);

            pool = pool->next;
            block = pool->first_block;

            continue;
        }
        if (!(block->free)) {
            //printf("Block not free\n");
            
            block = block->next;
            continue;
        }

        if (block->size == size) {
            //printf("Perfect fit\n");

            block->free = false;
            pool->free -= size;

            break;
        }

        if (pool->free < real_size) {
            //printf("Pool cannot fit me at all\n");

            if (pool->next == NULL)
                pool->next = mem_pool_create(size > DEFAULT_POOL_SIZE ? size : DEFAULT_POOL_SIZE);

            pool = pool->next;
            block = pool->first_block;
            
            continue;
        }

        if (block->size < real_size) {
            //printf("Block cannot fit me\n");

            block = block->next;
            continue;
        }
        
        //printf("I had to split a block\n");
        pool->free -= real_size;

	    //char stringbuffer[32];

        mem_block* new_block = (mem_block*)(((size_t)block) + real_size);

        //printf("Real Size: %s\n", itoa(real_size, stringbuffer, 10));

        new_block->size = block->size - real_size;
        new_block->free = true;
        new_block->parent = block->parent;
        new_block->next = block->next;
        
        block->size = size;
        block->free = false;
        block->next = new_block;

        break;
    }
    return (void*)(((size_t)block) + sizeof(mem_block));
}

void mem_pool_enum_blocks(mem_pool* pool) {

    mem_block* block = pool->first_block;

	char stringbuffer[32];

    while (block != NULL) {
        printf("Addr: 0x%s ", itoa((size_t)block,  stringbuffer, 16));
        printf("Size: %s ", itoa(block->size,  stringbuffer, 10));
        printf(block->free ? "Free" : "Busy");
        printf("\n");

        block = block->next;
    }
}

void mem_pool_cleanup(mem_pool* pool) {

    mem_block* block = pool->first_block;
    mem_block* next_block;

    while (block != NULL) {

        next_block = block->next;

        if (!(block->free)) {
            block = next_block;
            continue;
        }

        if (next_block == NULL)
            break;

        if (next_block->free) {
            block->size += sizeof(mem_block) + next_block->size;
            block->next = next_block->next;

            pool->size += sizeof(mem_block);

            continue;
        }
        block = next_block;
    }
}

void mem_pool_unalloc(void* space) {
    mem_block* block = (mem_block*)(((size_t)space) - sizeof(mem_block));
    block->free = true;

    block->parent->free += block->size;

    mem_pool_cleanup(block->parent);
}

char mem_pool_buffer[32];

void mem_pool_init() {

	mem_pool0 = mem_pool_create(DEFAULT_POOL_SIZE);
	printf("Created initial 1 MB kernel memory pool\n\n");

    //isr_stack = (size_t)mem_pool_alloc(8192);
	//printf("bong 0x%s\n", itoa(isr_stack, mem_pool_buffer, 16));
}