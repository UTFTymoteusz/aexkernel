#pragma once

#include "aex/mutex.h"
#include "aex/time.h"

#include <stddef.h>

typedef size_t phys_addr;

struct page_root {
    phys_addr root_dir;

    void* vstart;
    void* vend;

    uint64_t frames_used;
    uint64_t dir_frames_used;
    uint64_t map_frames_used;

    mutex_t mutex;
};
typedef struct page_root page_root_t;

void mempg_init_proot(page_root_t* proot, phys_addr root);