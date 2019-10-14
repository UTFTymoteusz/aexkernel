#include "aex/kmem.h"

#include "dev/cpu.h"

#include "mem/frame.h"
#include "mem/pagetrk.h"

#include "mem/page.h"

#include <stdio.h>
#include <string.h>

extern void* PML4;
extern void* PDPT1;
extern void* PDT1, * PDT2;

page_tracker_t kernel_pgtrk;

uint64_t* pgsptr;

void aim_pgsptr() {
    asm volatile("mov rax, cr3;\
                  mov cr3, rax;");
}

void mempg_init() {
    kernel_pgtrk.vstart = 0xFFFFFFFF80100000;
    kernel_pgtrk.root   = &PML4;

    mempg_init_tracker(&kernel_pgtrk, &PML4);
}
static void mempg_dir_trk_insert(page_tracker_t* tracker, uint32_t frame) {
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

// To do: Chicken-egg situation avoidance with paging
uint64_t* mempg_find_table_ensure(uint64_t virt_addr, page_tracker_t* tracker) {
    uint64_t pml4index = virt_addr >> 48 & 0x1FF;
    uint64_t pdpindex  = virt_addr >> 39 & 0x1FF;
    uint64_t pdindex   = virt_addr >> 30 & 0x1FF;

    uint32_t frame_id;

    volatile uint64_t* pml4 = (uint64_t*)tracker->root;
    if (!(pml4[pml4index] & 0b0001)) {
        printf("new_pdp ");
        frame_id = memfr_get_free();
        mempg_dir_trk_insert(tracker, frame_id);

        tracker->dir_frames_used++;

        pml4[pml4index] = ((uint64_t)memfr_alloc(frame_id)) | 0b11111;
    }
    else
        pml4[pml4index] |= 0b11111;

    uint64_t* pdp = (uint64_t*)(pml4[pml4index] & ~0xFFF);
    if (!(pdp[pdpindex] & 0b0001)) {
        printf("new_pd ");
        frame_id = memfr_get_free();
        mempg_dir_trk_insert(tracker, frame_id);

        tracker->dir_frames_used++;

        pdp[pdpindex] = ((uint64_t)memfr_alloc(frame_id)) | 0b11111;
    }
    else
        pdp[pdpindex] |= 0b11111;

    uint64_t* pd = (uint64_t*)(pdp[pdpindex] & ~0xFFF);
    if (!(pd[pdindex] & 0b0001)) {
        printf("new_pt ");
        frame_id = memfr_get_free();
        mempg_dir_trk_insert(tracker, frame_id);

        tracker->dir_frames_used++;

        pd[pdindex] = ((uint64_t)memfr_alloc(frame_id)) | 0b11111;
    }
    else
        pd[pdindex] |= 0b11111;

    uint64_t* pt = (uint64_t*)(pd[pdindex] & ~0xFFF);

    return (uint64_t*)(((uint64_t)pt) & ~0xFFF);
}

void mempg_assign(void* virt, void* phys, page_tracker_t* tracker, uint8_t flags) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    uint64_t virt_addr = ((uint64_t)virt) << 9;
    virt_addr &= MEM_PAGE_MASK;

    uint64_t phys_addr = (uint64_t)phys;
    phys_addr &= MEM_PAGE_MASK;

    uint64_t* table = mempg_find_table_ensure(virt_addr, tracker);
    uint64_t index = (virt_addr >> 21) & 0x1FF;

    table[index] = phys_addr | flags | 1;

    asm volatile("mov rax, cr3;\
                  mov cr3, rax;");
}

void mempg_remove(void* virt, page_tracker_t* tracker) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    uint64_t virt_addr = ((uint64_t)virt) << 9;
    virt_addr &= MEM_PAGE_MASK;

    uint64_t* table = mempg_find_table_ensure(virt_addr, tracker);
    uint64_t  index = (virt_addr >> 21) & 0x1FF;

    table[index] = 0;

    asm volatile("mov rax, cr3;\
                  mov cr3, rax;");
}

void mempg_trk_set(page_tracker_t* tracker, uint64_t id, uint32_t frame, uint32_t amount) {
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
        pointers[id++] = frame++;
        tracker->frames_used++;
        
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

void mempg_trk_mark(page_tracker_t* tracker, uint64_t id, uint32_t mark, uint32_t amount) {
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
        pointers[id++] = mark;
        tracker->frames_used++;
        
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

uint64_t mempg_trk_find(page_tracker_t* tracker, uint32_t amount) {
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
        if (combo >= amount)
            return ret;

        id++;
        idg++;
    }
}

void* mempg_alloc(size_t amount, page_tracker_t* tracker, uint8_t flags) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    uint64_t piece = mempg_trk_find(tracker, amount);
    uint32_t frame;

    void* phys_ptr;
    void* virt_ptr = ((void*)tracker->vstart) + (piece * MEM_FRAME_SIZE);

    void* start = virt_ptr;

    //printf("mempg_alloc() mapped piece %i, len %i\n", piece, amount);
    //for (volatile uint64_t i = 0; i < 45000000; i++);

    for (size_t i = 0; i < amount; i++) {
        frame    = memfr_get_free();
        phys_ptr = memfr_alloc(frame);

        mempg_assign(virt_ptr, phys_ptr, tracker, flags);
        mempg_trk_set(tracker, piece, frame, 1);

        piece++;

        virt_ptr += MEM_FRAME_SIZE;
    }
    return start;
}

void* mempg_calloc(size_t amount, page_tracker_t* tracker, uint8_t flags) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    uint32_t frame = memfr_calloc(amount);
    uint64_t piece = mempg_trk_find(tracker, amount);

    void* phys_ptr = memfr_get_ptr(frame);
    void* virt_ptr = ((void*)tracker->vstart) + (piece * MEM_FRAME_SIZE);

    void* start = virt_ptr;

    mempg_trk_set(tracker, piece, frame, amount);

    //printf("mempg_calloc() mapped piece %i, len %i\n", piece, amount);
    //for (volatile uint64_t i = 0; i < 45000000; i++);

    for (size_t i = 0; i < amount; i++) {
        mempg_assign(virt_ptr, phys_ptr, tracker, flags);

        phys_ptr += MEM_FRAME_SIZE;
        virt_ptr += MEM_FRAME_SIZE;
    }
    return start;
}

void mempg_unassoc(uint64_t id, size_t amount, page_tracker_t* tracker) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    mempg_trk_set(tracker, id, 0, amount);
}

void* mempg_mapto(size_t amount, void* phys_ptr, page_tracker_t* tracker, uint8_t flags) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    uint64_t piece = mempg_trk_find(tracker, amount);

    void* virt_ptr = ((void*)tracker->vstart) + (piece * MEM_FRAME_SIZE);
    void* start    = virt_ptr;

    mempg_trk_mark(tracker, piece, 0xFFFFFFFF, amount);

    printf("mempg_mapto() mapped piece %i (virt 0x%x), len %i to 0x%x\n", piece, (size_t)virt_ptr & 0xFFFFFFFFFFFF, amount, (size_t)phys_ptr & 0xFFFFFFFFFFFF);
    //for (volatile uint64_t i = 0; i < 45000000; i++);

    for (size_t i = 0; i < amount; i++) {
        mempg_assign(virt_ptr, phys_ptr, tracker, flags);

        phys_ptr += MEM_FRAME_SIZE;
        virt_ptr += MEM_FRAME_SIZE;
    }
    return start;
}

void* mempg_mapvto(size_t amount, void* virt_ptr, void* phys_ptr, page_tracker_t* tracker, uint8_t flags) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    uint64_t piece = mempg_trk_find(tracker, amount);

    mempg_trk_mark(tracker, piece, 0xFFFFFFFF, amount);

    void* start = virt_ptr;

    printf("mempg_mapvto() mapped piece %i (virt 0x%x), len %i to 0x%x\n", piece, (size_t)virt_ptr & 0xFFFFFFFFFFFF, amount, (size_t)phys_ptr & 0xFFFFFFFFFFFF);
    //for (volatile uint64_t i = 0; i < 45000000; i++);

    for (size_t i = 0; i < amount; i++) {
        mempg_assign(virt_ptr, phys_ptr, tracker, flags);

        phys_ptr += MEM_FRAME_SIZE;
        virt_ptr += MEM_FRAME_SIZE;
    }
    return start;
}

uint64_t mempg_indexof(void* virt, page_tracker_t* tracker) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    virt -= tracker->vstart;
    return ((uint64_t)virt) / MEM_FRAME_SIZE;
}

uint64_t mempg_frameof(void* virt, page_tracker_t* tracker) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    uint64_t id = mempg_indexof(virt, tracker);

    page_frame_ptrs_t* ptr = &(tracker->first);

    while (id >= PG_FRAME_POINTERS_PER_PIECE) {
        id -= PG_FRAME_POINTERS_PER_PIECE;
        ptr = ptr->next;
    }
    return ptr->pointers[id];
}

void* mempg_paddrof(void* virt, page_tracker_t* tracker) {
    if (tracker == NULL)
        tracker = &kernel_pgtrk;

    uint64_t virt_addr = ((uint64_t)virt) << 9;
    virt_addr &= MEM_PAGE_MASK;

    uint64_t* table = mempg_find_table_ensure(virt_addr, tracker);
    uint64_t index  = (virt_addr >> 21) & 0x1FF;

    return (void*)(table[index] & MEM_PAGE_MASK) + (((size_t)virt) & ~MEM_PAGE_MASK);
}

// Make cleanup code for this
void* mempg_create_user_root() {
    void* pg_dir = mempg_alloc(1, NULL, 0x03);

    memcpy(pg_dir, &PML4, 0x1000);

    uint64_t* pml4t = pg_dir;

    printf("0x%x ", pml4t[0]);
    printf("0x%x ", pml4t[1]);
    printf("0x%x ", pml4t[2]);
    printf("0x%x \n", pml4t[3]);

    printf("0x%x ", pml4t[508]);
    printf("0x%x ", pml4t[509]);
    printf("0x%x ", pml4t[510]);
    printf("0x%x \n", pml4t[511]);

    //memset(pml4t, 0, 0x1000);

    //pml4t[0] = 0;

    //printf("XD: 0x%x\n", pml4t[511] & 0xFFFFFFFFFFFF);

    return mempg_paddrof(pg_dir, NULL);
}