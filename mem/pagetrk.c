#pragma once

#include "aex/kmem.h"
#include "aex/klist.h"

struct page_availability {
    uint32_t assignments[1024];
};
typedef struct page_availability page_availability_t;

struct page_tracker {
    void* root;
    struct klist pages;
};
typedef struct page_tracker page_tracker_t;

void mempg_init_tracker(page_tracker_t* tracker, void* root) {
    memset(trk, 0, sizeof(page_tracker_t));

    trk->root = root;

    klist_init(&(trk->pages));
    return trk;
}