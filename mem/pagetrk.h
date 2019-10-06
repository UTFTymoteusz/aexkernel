#pragma once

#include "pagetrk.c"

// I need to think of a better name
struct page_availability;
typedef struct page_availability page_availability_t;

struct page_tracker;
typedef struct page_tracker page_tracker_t;

page_tracker_t* mempg_init_tracker(void* root);