#pragma once

#define PG_FRAME_POINTERS_PER_PIECE 4096

// I need to think of a better name
struct page_frame_ptrs {
    uint32_t pointers[PG_FRAME_POINTERS_PER_PIECE];
    struct page_frame_ptrs* next;
};
typedef struct page_frame_ptrs page_frame_ptrs_t;

struct page_tracker {
    void* root;
    size_t vstart;

    uint64_t frames_used;

    page_frame_ptrs_t first;
};
typedef struct page_tracker page_tracker_t;

void mempg_init_tracker(page_tracker_t* tracker, void* root);