#pragma once

#include "dev/cpu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MEM_FRAME_SIZE CPU_PAGE_SIZE

#define INTS_PER_PIECE 16362
#define FRAMES_PER_PIECE INTS_PER_PIECE * 32

struct memfr_alloc_piece {
    cpu_addr start;
    uint32_t usable;
    volatile uint32_t bitmap[INTS_PER_PIECE];
    struct memfr_alloc_piece* next;
    uint16_t padding;
} __attribute__((packed));
typedef struct memfr_alloc_piece memfr_alloc_piece_t;

memfr_alloc_piece_t memfr_alloc_piece0;

// Allocates the specified frame id.
void* memfr_alloc(uint32_t frame_id);

// Unallocates the specified frame id.
bool  memfr_unalloc(uint32_t frame_id);

// Finds a free frame id.
uint64_t memfr_amount();
uint64_t memfr_used();

// Checks if a frame is taken.
bool  memfr_isfree(uint32_t frame_id);

// Gets the physical address of a frame.
void* memfr_get_ptr(uint32_t frame_id);

// Allocates a physically contiguous amount of frames. Returns the id of the first allocated frame.
uint32_t memfr_calloc(uint32_t amount);