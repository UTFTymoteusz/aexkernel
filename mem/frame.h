#pragma once

#include "frame.c"

struct memfr_alloc_piece;
typedef struct memfr_alloc_piece memfr_alloc_piece_t;

memfr_alloc_piece_t memfr_alloc_piece0;

void* memfr_alloc(uint32_t id);
bool memfr_unalloc(uint32_t id);

uint32_t memfr_get_free();
uint64_t memfr_amount();

bool memfr_isfree(uint32_t id);
void* memfr_get_ptr(uint32_t id);

uint32_t memfr_calloc(uint32_t amount);