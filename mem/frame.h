#pragma once

#include "dev/cpu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MEM_FRAME_SIZE CPU_PAGE_SIZE

#define INTS_PER_PIECE 1010
#define FRAMES_PER_PIECE INTS_PER_PIECE * 32

struct memfr_alloc_piece {
    cpu_addr start;
    uint16_t usable;
    uint32_t bitmap[INTS_PER_PIECE];
    struct memfr_alloc_piece* next;
    uint16_t padding;
} __attribute__((packed));
typedef struct memfr_alloc_piece memfr_alloc_piece_t;

memfr_alloc_piece_t memfr_alloc_piece0;

void* memfr_alloc(uint32_t id);
bool  memfr_unalloc(uint32_t id);

uint32_t memfr_get_free();
uint64_t memfr_amount();
uint64_t memfr_used();

bool  memfr_isfree(uint32_t id);
void* memfr_get_ptr(uint32_t id);

uint32_t memfr_calloc(uint32_t amount);