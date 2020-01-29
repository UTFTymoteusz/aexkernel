#include "aex/mem.h"
#include "aex/klist.h"
#include "aex/string.h"

#include "pagetrk.h"

struct page_frame_ptrs;
typedef struct page_frame_ptrs page_frame_ptrs_t;

struct page_root;
typedef struct page_root page_root_t;

void mempg_init_proot(page_root_t* proot, phys_addr root) {
    proot->root_dir = root;
    proot->frames_used     = 0;
    proot->dir_frames_used = 0;
    proot->mutex.val  = 0;
    proot->mutex.name = "page tracker";
}