#pragma once

#include "aex/klist.h"

// I need to think of a better name
struct page_availability {
    uint32_t assignments[1024];
};
typedef struct page_availability page_availability_t;

struct page_tracker {
    void* root;
    struct klist pages;
};
typedef struct page_tracker page_tracker_t;

void mempg_init_tracker(page_tracker_t* tracker, void* root);