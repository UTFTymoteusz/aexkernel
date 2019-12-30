#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aex/kernel.h"
#include "aex/mutex.h"
#include "aex/sys.h"

#include "aex/dev/cpu.h"

#include "boot/multiboot.h"

#include "frame.h"

#define FRAMES_PER_INT 32

struct memfr_alloc_piece;
typedef struct memfr_alloc_piece memfr_alloc_piece_t;

// Memory stuff
uint64_t frame_current = 0;
uint64_t frame_last    = 0;
uint64_t frames_possible = 0;

uint64_t frames_used = 0;

mutex_t fr_mutex = 0;

memfr_alloc_piece_root_t memfr_alloc_piece0;

void* memfr_alloc_internal(uint32_t id) {
    memfr_alloc_piece_t* piece = (memfr_alloc_piece_t*) &memfr_alloc_piece0;
    uint32_t fr = id;

    while (true)
        if (id < piece->usable) {
            if (!(piece->bitmap[id / FRAMES_PER_INT] & (1UL << (id % FRAMES_PER_INT))))
                ++frames_used;
            else
                printk("False alloc of frame %i\n", fr);

            piece->bitmap[id / FRAMES_PER_INT] |= 1UL << (id % FRAMES_PER_INT);
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

void* kfalloc(uint32_t id) {
    mutex_acquire(&fr_mutex);
    void* ret = memfr_alloc_internal(id);

    mutex_release(&fr_mutex);
    return ret;
}

bool kffree(uint32_t id) {
    memfr_alloc_piece_t* piece = (memfr_alloc_piece_t*) &memfr_alloc_piece0;
    uint32_t fr = id;

    mutex_acquire(&fr_mutex);

    while (true)
        if (id < piece->usable) {
            if (piece->bitmap[id / FRAMES_PER_INT] & (1UL << (id % FRAMES_PER_INT)))
                --frames_used;
            else
                printk("False unalloc of frame %i\n", fr);

            piece->bitmap[id / FRAMES_PER_INT] &= ~(1UL << (id % FRAMES_PER_INT));
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

uint64_t kfamount() {
    return frames_possible;
}

uint64_t kfused() {
    return frames_used;
}

bool kfisfree(uint32_t id) {
    memfr_alloc_piece_t* piece = (memfr_alloc_piece_t*) &memfr_alloc_piece0;

    while (true)
        if (id < piece->usable)
            return (piece->bitmap[id / FRAMES_PER_INT] & (1UL << (id % FRAMES_PER_INT))) == 0;
        else {
            id -= piece->usable;
            piece = piece->next;

            if (piece == NULL || id > frames_possible)
                kpanic("Failed to unallocate a memory frame");
        }

    return false;
}

void* kfpaddrof(uint32_t id) {
    memfr_alloc_piece_t* piece = (memfr_alloc_piece_t*) &memfr_alloc_piece0;

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

// optimize this later on cuz now am too lazy
uint32_t kfcalloc(uint32_t amount) {
    if (amount == 0)
        return 0;

    mutex_acquire(&fr_mutex);

    uint32_t start_id = 0;
    uint32_t combo = 0;

    for (uint32_t i = 0; i < frames_possible; i++) {
        if (start_id == 0 && kfisfree(i))
            start_id = i;

        if (!kfisfree(i)) {
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