#include "aex/mem.h"
#include "aex/klist.h"

#include <string.h>

#include "pagetrk.h"

struct page_frame_ptrs;
typedef struct page_frame_ptrs page_frame_ptrs_t;

struct page_tracker;
typedef struct page_tracker page_tracker_t;

void mempg_init_tracker(page_tracker_t* tracker, void* root) {
    tracker->root = root;
    tracker->frames_used     = 0;
    tracker->dir_frames_used = 0;
    tracker->mutex = 0;

    memset(&(tracker->first)    , 0, sizeof(page_frame_ptrs_t));
    memset(&(tracker->dir_first), 0, sizeof(page_frame_ptrs_t));
}