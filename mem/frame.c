#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "aex/mutex.h"

#include "boot/multiboot.h"
#include "dev/cpu.h"
#include "kernel/sys.h"

#include "frame.h"

struct memfr_alloc_piece;
typedef struct memfr_alloc_piece memfr_alloc_piece_t;

// Memory stuff
uint64_t frame_current = 0;
uint64_t frame_last    = 0;
uint64_t frames_possible = 0;

uint64_t frames_used = 0;

mutex_t fr_mutex = 0;

memfr_alloc_piece_t memfr_alloc_piece0;

void* memfr_alloc_internal(uint32_t id) {
    memfr_alloc_piece_t* piece = &memfr_alloc_piece0;
    uint32_t fr = id;

    while (true)
        if (id < piece->usable) {
            if (!(piece->bitmap[id / 32] & (1UL << id % 32)))
                ++frames_used;
            else
                printf("False alloc of frame %i\n", fr);

            piece->bitmap[id / 32] |= 1UL << id % 32;
            return (void*) piece->start + id * CPU_PAGE_SIZE;
        }
        else {
            id -= piece->usable;
            piece = piece->next;

            if (piece == NULL || id > frames_possible)
                kpanic("Failed to allocate a memory frame");
        }

    return NULL;
}

void* memfr_alloc(uint32_t id) {
    mutex_acquire(&fr_mutex);
    void* ret = memfr_alloc_internal(id);

    mutex_release(&fr_mutex);
    return ret;
}

bool memfr_unalloc(uint32_t id) {
    memfr_alloc_piece_t* piece = &memfr_alloc_piece0;
    uint32_t fr = id;
    
    mutex_acquire(&fr_mutex);

    while (true)
        if (id < piece->usable) {
            if (piece->bitmap[id / 32] & ~(1UL << id % 32))
                --frames_used;
            else
                printf("False unalloc of frame %i\n", fr);
            
            piece->bitmap[id / 32] &= ~(1UL << id % 32);
            mutex_release(&fr_mutex);
            return true;
        }
        else {
            id -= piece->usable;
            piece = piece->next;

            if (piece == NULL || id > frames_possible)
                kpanic("Failed to unallocate a memory frame");
        }

    mutex_release(&fr_mutex);
    return false;
}

uint32_t memfr_get_free() {
    memfr_alloc_piece_t* piece = &memfr_alloc_piece0;

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

            if (piece == NULL || id > frames_possible)
                kpanic("Failed to get a free memory frame");
        }
        ++id;
        ++id_ret;
    }
    return 0;
}

uint64_t memfr_amount() {
    return frames_possible;
}

bool memfr_isfree(uint32_t id) {
    memfr_alloc_piece_t* piece = &memfr_alloc_piece0;

    while (true)
        if (id < piece->usable)
            return (piece->bitmap[id / 32] & (1UL << id % 32)) == 0;
        else {
            id -= piece->usable;
            piece = piece->next;

            if (piece == NULL || id > frames_possible)
                kpanic("Failed to unallocate a memory frame");
        }

    return false;
}

void* memfr_get_ptr(uint32_t id) {
    memfr_alloc_piece_t* piece = &memfr_alloc_piece0;

    while (true) {
        if (id < piece->usable)
            return (void*) piece->start + id * CPU_PAGE_SIZE;
        else {
            id -= piece->usable;
            piece = piece->next;

            if (piece == NULL || id > frames_possible)
                kpanic("Failed to get pointer to a memory frame");
        }
    }
    return NULL;
}

uint32_t memfr_calloc(uint32_t amount) {
    if (amount == 0)
        return 0;

    mutex_acquire(&fr_mutex);

    uint32_t start_id = 0;
    uint32_t combo = 0;

    for (uint32_t i = 0; i < frames_possible; i++) {
        if (start_id == 0 && memfr_isfree(i))
            start_id = i;

        if (!memfr_isfree(i)) {
            start_id = 0;
            combo = 0;

            continue;
        }
        else {
            ++combo;

            if (combo == amount) {
                for (i = 0; i < amount; i++)
                    memfr_alloc_internal(start_id + i);

                mutex_release(&(fr_mutex));
                return start_id;
            }
        }
    }
    mutex_release(&(fr_mutex));
    kpanic("Failed to allocate contiguous frames");
    return 0;
}

uint64_t memfr_used() {
    return frames_used;
}