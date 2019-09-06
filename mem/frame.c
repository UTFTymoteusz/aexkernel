#pragma once

#include <stdio.h>
#include <string.h>

#include "boot/multiboot.h"
#include "dev/cpu.h"
#include "kernel/sys.h"

#define MEM_FRAME_SIZE CPU_PAGE_SIZE

#define INTS_PER_PIECE 1010
#define FRAMES_PER_PIECE INTS_PER_PIECE * 32

typedef struct mem_frame_alloc_piece {
    addr start;
    uint16_t usable;
    uint32_t bitmap[INTS_PER_PIECE];
    struct mem_frame_alloc_piece* next;
    uint16_t padding;
} __attribute__((packed)) mem_frame_alloc_piece;

// Memory stuff
uint64_t frame_current = 0;
uint64_t frame_last = 0;
uint64_t frames_possible = 0;

mem_frame_alloc_piece mem_frame_alloc_piece0;

void* mem_frame_alloc(uint32_t id) {

    mem_frame_alloc_piece* piece = &mem_frame_alloc_piece0;

    while (true) {

        if (id < piece->usable) {
            piece->bitmap[id / 32] |= 1UL << id % 32;

            return (void*)piece->start + id * CPU_PAGE_SIZE;
        }
        else {
            id -= piece->usable;
            piece = piece->next;

            if (piece == NULL)
                kpanic("Failed to allocate a memory frame");
        }
    }
    return NULL;
}
bool mem_frame_unalloc(uint32_t id) {

    mem_frame_alloc_piece* piece = &mem_frame_alloc_piece0;

    while (true) {

        if (id < piece->usable) {
            piece->bitmap[id / 32] &= ~(1UL << id % 32);

            return true;
        }
        else {
            id -= piece->usable;
            piece = piece->next;

            if (piece == NULL)
                kpanic("Failed to unallocate a memory frame");
        }
    }
    return false;
}
uint32_t mem_frame_get_free() {

    mem_frame_alloc_piece* piece = &mem_frame_alloc_piece0;

    uint32_t id_ret = 0;
    uint32_t id = 0;

    while (true) {

        if (id < piece->usable) {

            if (!(piece->bitmap[id / 32] & (1UL << id % 32)))
                return id_ret;
        }
        else {
            id -= piece->usable;
            piece = piece->next;

            if (piece == NULL)
                kpanic("Failed to get a free memory frame");
        }
        ++id;
        ++id_ret;
    }
    return 0;
}
uint64_t mem_frame_amount() {
    return frames_possible;
}
bool mem_frame_isfree(uint32_t id) {

    mem_frame_alloc_piece* piece = &mem_frame_alloc_piece0;

    while (true) {

        if (id < piece->usable)
            return (piece->bitmap[id / 32] & (1UL << id % 32)) == 0;
        else {
            id -= piece->usable;
            piece = piece->next;

            if (piece == NULL)
                kpanic("Failed to unallocate a memory frame");
        }
    }
    return false;
}
void* mem_frame_get_ptr(uint32_t id) {

    mem_frame_alloc_piece* piece = &mem_frame_alloc_piece0;

    while (true) {

        if (id < piece->usable)
            return (void*)piece->start + id * CPU_PAGE_SIZE;
        else {
            id -= piece->usable;
            piece = piece->next;

            if (piece == NULL)
                kpanic("Failed to get pointer to a memory frame");
        }
    }
    return NULL;
}
uint32_t mem_frame_alloc_contiguous(uint32_t amount) {

    if (amount == 0)
        return 0;

    //disableinterrupts();
    uint32_t start_id = 0;
    uint32_t combo = 0;

    for (uint32_t i = 0; i < frames_possible; i++) {

        if (!mem_frame_isfree(i)) {
            start_id = 0;
            combo = 0;

            continue;
        }
        else {
            ++combo;

            if (combo >= amount) {

                for (i = 0; i < amount; i++)
                    mem_frame_alloc(start_id + i);

                return start_id;
            }
        }

        if (start_id == 0 && mem_frame_isfree(i))
            start_id = i;
    }
    //enableinterrupts();

    kpanic("Failed to allocate contiguous frames");

    return 0;
}