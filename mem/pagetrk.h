#pragma once

#include "aex/mutex.h"
#include "aex/time.h"

#define PG_FRAME_POINTERS_PER_PIECE 2048

// I need to think of a better name
struct page_frame_ptrs {
    uint32_t pointers[PG_FRAME_POINTERS_PER_PIECE];
    struct page_frame_ptrs* next;
};
typedef struct page_frame_ptrs page_frame_ptrs_t;

struct page_tracker {
    void*  root;
    size_t root_virt;
    size_t vstart;

    uint64_t frames_used;
    uint64_t dir_frames_used;
    uint64_t map_frames_used;

    mutex_t mutex;

    page_frame_ptrs_t first;
    page_frame_ptrs_t dir_first;
};
typedef struct page_tracker page_tracker_t;

void mempg_init_tracker(page_tracker_t* tracker, void* root);