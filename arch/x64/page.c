#include "aex/mem.h"
#include "aex/mutex.h"
#include "aex/sys.h"
#include "aex/syscall.h"

#include "mem/frame.h"
#include "mem/pagetrk.h"

#include "mem/page.h"

#include <stdio.h>
#include <string.h>

extern void* PML4;
extern void* PDPT1;
extern void* PDT1,* PDT2;

page_tracker_t kernel_pgtrk;

volatile uint64_t* pgsptr;
volatile uint64_t* pg_page_entry = NULL;

static inline void aim_pgsptr(void* at) {
    if (pg_page_entry == NULL) {
        pgsptr = (uint64_t*) at;
        return;
    }
    uint64_t addr = (((uint64_t) at) & MEM_PAGE_MASK) | 0x03;
    if (*pg_page_entry == addr)
        return;

    *pg_page_entry = addr;

    asm volatile("mov rax, cr3;\
                  mov cr3, rax;");
}

void mempg_init() {
    kernel_pgtrk.vstart = 0xFFFFFFFF80100000;
    kernel_pgtrk.root   = &PML4;

    mempg_init_tracker(&kernel_pgtrk, &PML4);
    kernel_pgtrk.dir_frames_used = 8;
}

void mempg_init2() {
    void* virt_addr = kpmap(1, (void*) 0x100000, NULL, 0x03);
    uint64_t virt = (uint64_t) virt_addr;

    uint64_t pml4index = (virt >> 39) & 0x1FF;
    uint64_t pdpindex  = (virt >> 30) & 0x1FF;
    uint64_t pdindex   = (virt >> 21) & 0x1FF;
    uint64_t ptindex   = (virt >> 12) & 0x1FF;

    volatile uint64_t* pml4 = kernel_pgtrk.root;
    volatile uint64_t* pdp  = (uint64_t*) (pml4[pml4index] & ~0xFFF);
    volatile uint64_t* pd   = (uint64_t*) (pdp[pdpindex] & ~0xFFF);
    volatile uint64_t* pt   = (uint64_t*) (pd[pdindex] & ~0xFFF);

    volatile uint64_t* pt_v  = kpmap(1, (void*) pt, NULL, 0x03);

    pg_page_entry = (uint64_t*) (pt_v + ptindex);

    pgsptr = (void*) (virt & MEM_PAGE_MASK);
    
    for (int i = 0; i < 16; i++)
        ((uint64_t*) kernel_pgtrk.root)[i] = 0x0000; // We don't need these anymore

    syscalls[SYSCALL_PGALLOC] = syscall_pgalloc;
    syscalls[SYSCALL_PGFREE]  = syscall_pgfree;
}

static inline void mempg_dir_trk_insert(page_tracker_t* tracker, uint32_t frame) {
    page_frame_ptrs_t* ptr = &(tracker->dir_first);

    uint32_t* pointers = ptr->pointers;
    uint64_t  id = 0;

    while (true) {
        for (id = 0; id < PG_FRAME_POINTERS_PER_PIECE; id++)
            if (pointers[id] == 0) {
                pointers[id] = frame;
                return;
            }

        if (ptr->next == NULL) {
            ptr->next = kmalloc(sizeof(page_frame_ptrs_t));
            memset(ptr->next, 0, sizeof(page_frame_ptrs_t));
        }
        ptr      = ptr->next;
        pointers = ptr->pointers;
    }
}

static inline uint64_t* mempg_find_table_ensure(uint64_t virt_addr, page_tracker_t* tracker) {
    uint64_t pml4index = (virt_addr >> 48) & 0x1FF;
    uint64_t pdpindex  = (virt_addr >> 39) & 0x1FF;
    uint64_t pdindex   = (virt_addr >> 30) & 0x1FF;

    bool reset = false;

    uint32_t frame_id;

    aim_pgsptr(tracker->root);
    volatile uint64_t* pml4 = pgsptr;

    if (!(pml4[pml4index] & 0b0001)) {
        frame_id = kfcalloc(1);
        mempg_dir_trk_insert(tracker, frame_id);

        tracker->dir_frames_used++;

        pml4[pml4index] = ((uint64_t) kfpaddrof(frame_id)) | 0b00111;
        reset = true;
    }
    else
        pml4[pml4index] |= 0b00111;

    aim_pgsptr((void*) (pml4[pml4index] & ~0xFFF));
    volatile uint64_t* pdp = pgsptr;
    if (reset) {
        memset((void*) pdp, 0, 0x1000);
        reset = false;
    }

    if (!(pdp[pdpindex] & 0b0001)) {
        frame_id = kfcalloc(1);
        mempg_dir_trk_insert(tracker, frame_id);

        tracker->dir_frames_used++;

        pdp[pdpindex] = ((uint64_t) kfpaddrof(frame_id)) | 0b00111;
        reset = true;
    }
    else
        pdp[pdpindex] |= 0b00111;

    aim_pgsptr((void*) (pdp[pdpindex] & ~0xFFF));
    volatile uint64_t* pd = (uint64_t*) pgsptr;
    if (reset) {
        memset((void*) pd, 0, 0x1000);
        reset = false;
    }

    if (!(pd[pdindex] & 0b0001)) {
        frame_id = kfcalloc(1);
        mempg_dir_trk_insert(tracker, frame_id);

        tracker->dir_frames_used++;

        pd[pdindex] = ((uint64_t) kfpaddrof(frame_id)) | 0b00111;
        reset = true;
    }
    else
        pd[pdindex] |= 0b00111;

    aim_pgsptr((void*) (pd[pdindex] & ~0xFFF));
    volatile uint64_t* pt = (uint64_t*) pgsptr;

    if (reset) {
        memset((void*) pt, 0, 0x1000);
        reset = false;
    }
    return (uint64_t*) (((uint64_t) pt) & ~0xFFF);
}

void mempg_assign(void* virt, void* phys, page_tracker_t* tracker, uint8_t flags) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    uint64_t virt_addr = ((uint64_t) virt) << 9;
    virt_addr &= MEM_PAGE_MASK;

    uint64_t phys_addr = (uint64_t) phys;
    phys_addr &= MEM_PAGE_MASK;

    uint64_t* volatile table = mempg_find_table_ensure(virt_addr, tracker);
    uint64_t index  = (virt_addr >> 21) & 0x1FF;

    table[index] = phys_addr | flags | 1;

    asm volatile("mov rax, cr3;\
                  mov cr3, rax;");
}

void mempg_remove(void* virt, page_tracker_t* tracker) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    uint64_t virt_addr = ((uint64_t) virt) << 9;
    virt_addr &= MEM_PAGE_MASK;

    uint64_t* volatile table = mempg_find_table_ensure(virt_addr, tracker);
    uint64_t index = (virt_addr >> 21) & 0x1FF;

    table[index] = 0;

    asm volatile("mov rax, cr3;\
                  mov cr3, rax;");
}

static inline void mempg_trk_set(page_tracker_t* tracker, uint64_t id, uint32_t frame, uint32_t amount) {
    page_frame_ptrs_t* ptr = &(tracker->first);

    while (id >= PG_FRAME_POINTERS_PER_PIECE) {
        id -= PG_FRAME_POINTERS_PER_PIECE;

        if (ptr->next == NULL) {
            ptr->next = kmalloc(sizeof(page_frame_ptrs_t));
            memset(ptr->next, 0, sizeof(page_frame_ptrs_t));
        }
        ptr = ptr->next;
    }
    uint32_t* pointers = ptr->pointers;

    tracker->frames_used += amount;

    while (amount-- > 0) {
        pointers[id++] = frame++;

        if (id >= PG_FRAME_POINTERS_PER_PIECE) {
            id -= PG_FRAME_POINTERS_PER_PIECE;

            if (ptr->next == NULL) {
                ptr->next = kmalloc(sizeof(page_frame_ptrs_t));
                memset(ptr->next, 0, sizeof(page_frame_ptrs_t));
            }
            ptr      = ptr->next;
            pointers = ptr->pointers;
        }
    }
}

static inline void mempg_trk_unalloc(page_tracker_t* tracker, uint64_t id, uint32_t amount) {
    page_frame_ptrs_t* ptr = &(tracker->first);

    while (id >= PG_FRAME_POINTERS_PER_PIECE) {
        id -= PG_FRAME_POINTERS_PER_PIECE;

        if (ptr->next == NULL) {
            ptr->next = kmalloc(sizeof(page_frame_ptrs_t));
            memset(ptr->next, 0, sizeof(page_frame_ptrs_t));
        }
        ptr = ptr->next;
    }
    uint32_t* pointers = ptr->pointers;

    while (amount-- > 0) {
        mempg_remove((void*) (id * MEM_FRAME_SIZE), tracker);

        if (pointers[id] > 0) {
            tracker->frames_used--;
            kffree(pointers[id]);
            pointers[id] = 0;
        }
        else
            printf("incorrect attempt at free of %i\n", id);

        ++id;

        if (id >= PG_FRAME_POINTERS_PER_PIECE) {
            id -= PG_FRAME_POINTERS_PER_PIECE;

            if (ptr->next == NULL) {
                ptr->next = kmalloc(sizeof(page_frame_ptrs_t));
                memset(ptr->next, 0, sizeof(page_frame_ptrs_t));
            }
            ptr      = ptr->next;
            pointers = ptr->pointers;
        }
    }
}

static inline void mempg_trk_mark(page_tracker_t* tracker, uint64_t id, uint32_t mark, uint32_t amount) {
    page_frame_ptrs_t* ptr = &(tracker->first);

    while (id >= PG_FRAME_POINTERS_PER_PIECE) {
        id -= PG_FRAME_POINTERS_PER_PIECE;

        if (ptr->next == NULL) {
            ptr->next = kmalloc(sizeof(page_frame_ptrs_t));
            memset(ptr->next, 0, sizeof(page_frame_ptrs_t));
        }
        ptr = ptr->next;
    }
    uint32_t* pointers = ptr->pointers;

    if (mark != 0)
        tracker->frames_used += amount;
    else
        tracker->frames_used -= amount;

    if (mark == 0xFFFFFFFF)
        tracker->map_frames_used += amount;

    while (amount-- > 0) {
        if (pointers[id] == 0xFFFFFFFF)
            tracker->map_frames_used--;

        pointers[id++] = mark;

        if (id >= PG_FRAME_POINTERS_PER_PIECE) {
            id -= PG_FRAME_POINTERS_PER_PIECE;

            if (ptr->next == NULL) {
                ptr->next = kmalloc(sizeof(page_frame_ptrs_t));
                memset(ptr->next, 0, sizeof(page_frame_ptrs_t));
            }
            ptr      = ptr->next;
            pointers = ptr->pointers;
        }
    }
}

static inline uint64_t mempg_trk_find(page_tracker_t* tracker, uint32_t amount) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    page_frame_ptrs_t* ptr = &(tracker->first);

    uint32_t* pointers = ptr->pointers;

    uint32_t combo = 0;
    uint64_t ret   = 0;
    uint64_t id    = 0;
    uint64_t idg   = 0;

    while (true) {
        if (id >= PG_FRAME_POINTERS_PER_PIECE) {
            id -= PG_FRAME_POINTERS_PER_PIECE;

            if (ptr->next == NULL) {
                ptr->next = kmalloc(sizeof(page_frame_ptrs_t));
                memset(ptr->next, 0, sizeof(page_frame_ptrs_t));
            }
            ptr      = ptr->next;
            pointers = ptr->pointers;
        }
        if (pointers[id] != 0) {
            combo = 0;
            ret   = 0;

            id++;
            idg++;

            continue;
        }
        else {
            if (combo == 0)
                ret = idg;

            combo++;
        }

        if (combo > amount) { // May be buggy, fix in the future pls
            //printf("pointer: %i\n", pointers[id]);
            //printf("combo: %i, amount: %i\n", combo, amount);
            return ret;
        }
        id++;
        idg++;
    }
}

void* kpalloc(size_t amount, page_tracker_t* tracker, uint8_t flags) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;
        
    mutex_acquire(&(tracker->mutex));

    uint64_t piece = mempg_trk_find(tracker, amount);
    uint32_t frame;

    void* phys_ptr;
    void* virt_ptr = ((void*) tracker->vstart) + (piece * MEM_FRAME_SIZE);

    void* start = virt_ptr;
    size_t first = 0;

    for (size_t i = 0; i < amount; i++) {
        frame    = kfcalloc(1);
        phys_ptr = kfpaddrof(frame);

        if (first == 0)
            first = (size_t) phys_ptr;

        mempg_assign(virt_ptr, phys_ptr, tracker, flags);
        mempg_trk_set(tracker, piece, frame, 1);
        piece++;

        virt_ptr += MEM_FRAME_SIZE;
    }
    mutex_release(&(tracker->mutex));
    return start;
}

void* kpcalloc(size_t amount, page_tracker_t* tracker, uint8_t flags) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    mutex_acquire(&(tracker->mutex));
    uint32_t frame = kfcalloc(amount);
    uint64_t piece = mempg_trk_find(tracker, amount);

    void* phys_ptr = kfpaddrof(frame);
    void* virt_ptr = ((void*) tracker->vstart) + (piece * MEM_FRAME_SIZE);

    void* start = virt_ptr;

    mempg_trk_set(tracker, piece, frame, amount);

    for (size_t i = 0; i < amount; i++) {
        mempg_assign(virt_ptr, phys_ptr, tracker, flags);

        phys_ptr += MEM_FRAME_SIZE;
        virt_ptr += MEM_FRAME_SIZE;
    }
    mutex_release(&(tracker->mutex));
    return start;
}

void kpfree(void* virt, size_t amount, page_tracker_t* tracker) {
    uint64_t id = kpindexof(virt, NULL);

    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    if (((size_t) virt & 0xFFF) > 0)
        kpanic("free alignment");

    mutex_acquire(&(tracker->mutex));
    mempg_trk_unalloc(tracker, id, amount);
    mutex_release(&(tracker->mutex));
}

void kpunmap(void* virt, size_t amount, page_tracker_t* tracker) {
    uint64_t id = kpindexof(virt, NULL);

    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    if (((size_t) virt & 0xFFF) > 0)
        kpanic("unmap alignment");

    mutex_acquire(&(tracker->mutex));
    mempg_trk_mark(tracker, id, 0, amount);
    mutex_release(&(tracker->mutex));
}

void* kpmap(size_t amount, void* phys_ptr, page_tracker_t* tracker, uint8_t flags) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    mutex_acquire(&(tracker->mutex));

    uint64_t piece = mempg_trk_find(tracker, amount);
    size_t offset = 0;

    if (((size_t) phys_ptr & 0xFFF) > 0) {
        amount++;
        offset = (size_t) phys_ptr & 0xFFF;
    }

    void* virt_ptr = ((void*) tracker->vstart) + (piece * MEM_FRAME_SIZE);
    void* start    = virt_ptr;

    mempg_trk_mark(tracker, piece, 0xFFFFFFFF, amount);

    for (size_t i = 0; i < amount; i++) {
        mempg_assign(virt_ptr, phys_ptr - offset, tracker, flags);

        phys_ptr += MEM_FRAME_SIZE;
        virt_ptr += MEM_FRAME_SIZE;
    }
    mutex_release(&(tracker->mutex));
    return start + offset;
}

void* mempg_mapvto(size_t amount, void* virt_ptr, void* phys_ptr, page_tracker_t* tracker, uint8_t flags) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    mutex_acquire(&(tracker->mutex));

    uint64_t piece = mempg_trk_find(tracker, amount);

    mempg_trk_mark(tracker, piece, 0xFFFFFFFF, amount);

    void* start = virt_ptr;

    printf("mempg_mapvto() mapped piece %i (virt 0x%x), len %i to 0x%x\n", piece, (size_t) virt_ptr & 0xFFFFFFFFFFFF, amount, (size_t) phys_ptr & 0xFFFFFFFFFFFF);
    //for (volatile uint64_t i = 0; i < 45000000; i++);

    for (size_t i = 0; i < amount; i++) {
        mempg_assign(virt_ptr, phys_ptr, tracker, flags);

        phys_ptr += MEM_FRAME_SIZE;
        virt_ptr += MEM_FRAME_SIZE;
    }
    mutex_release(&(tracker->mutex));
    return start;
}

uint64_t kpindexof(void* virt, page_tracker_t* tracker) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    virt -= tracker->vstart;
    return ((uint64_t) virt) / MEM_FRAME_SIZE;
}

uint64_t kpframeof(void* virt, page_tracker_t* tracker) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    uint64_t id = kpindexof(virt, tracker);

    page_frame_ptrs_t* ptr = &(tracker->first);

    while (id >= PG_FRAME_POINTERS_PER_PIECE) {
        id -= PG_FRAME_POINTERS_PER_PIECE;
        ptr = ptr->next;
    }
    return ptr->pointers[id];
}

void* kppaddrof(void* virt, page_tracker_t* tracker) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    uint64_t virt_addr = ((uint64_t) virt) << 9;
    virt_addr &= MEM_PAGE_MASK;

    mutex_acquire(&(tracker->mutex));

    uint64_t* table = mempg_find_table_ensure(virt_addr, tracker);
    uint64_t index  = (virt_addr >> 21) & 0x1FF;

    void* ret = (void*) (table[index] & MEM_PAGE_MASK) + (((size_t) virt) & ~MEM_PAGE_MASK);
    mutex_release(&(tracker->mutex));

    return ret;
}

void* mempg_create_user_root(size_t* virt_addr) {
    void* pg_dir = kpalloc(1, NULL, 0x03);
    *virt_addr = (size_t) pg_dir;

    memset(pg_dir, 0, 0x1000);

    uint64_t* pml4t = pg_dir;
    pml4t[511] = (uint64_t) (&PDPT1) | 0x03;

    return kppaddrof(pg_dir, NULL);
}

void mempg_dispose_user_root(size_t virt_addr) {
    kpfree((void*) virt_addr, 1, NULL);
}

void mempg_set_pagedir(page_tracker_t* tracker) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    task_current_context->cr3 = tracker->root;
    asm volatile("mov cr3, %0;" : : "r" (tracker->root));
}