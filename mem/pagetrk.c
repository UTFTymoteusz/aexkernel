#include "aex/kmem.h"
#include "aex/klist.h"

#include <string.h>

#include "pagetrk.h"

struct page_availability;
typedef struct page_availability page_availability_t;

struct page_tracker;
typedef struct page_tracker page_tracker_t;

void mempg_init_tracker(page_tracker_t* tracker, void* root) {
    memset(tracker, 0, sizeof(page_tracker_t));

    tracker->root = root;

    klist_init(&(tracker->pages));
}