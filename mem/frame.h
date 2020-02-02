#pragma once

#include "aex/aex.h"

#include "aex/sys/cpu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MEM_FRAME_SIZE CPU_PAGE_SIZE

struct memfr_alloc_piece {
    phys_addr start;
    uint32_t usable;
    struct memfr_alloc_piece* next;
    uint32_t bitmap[];
};
typedef struct memfr_alloc_piece memfr_alloc_piece_t;

struct memfr_alloc_piece_root {
    phys_addr start;
    uint32_t usable;
    struct memfr_alloc_piece* next;
    uint32_t bitmap[(MEM_FRAME_SIZE - sizeof(memfr_alloc_piece_t)) / sizeof(uint32_t)];
};
typedef struct memfr_alloc_piece_root memfr_alloc_piece_root_t;

memfr_alloc_piece_root_t memfr_alloc_piece0;