#pragma once

#include "mem/frame.c"

mem_frame_alloc_piece mem_frame_alloc_piece0;

void* mem_frame_alloc(uint32_t id);
bool mem_frame_unalloc(uint32_t id);

uint32_t mem_frame_get_free();
uint64_t mem_frame_amount();

bool mem_frame_isfree(uint32_t id);
void* mem_frame_get_ptr(uint32_t id);


uint32_t mem_frame_alloc_contiguous(uint32_t amount);