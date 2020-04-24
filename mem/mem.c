#include "aex/kernel.h"
#include "aex/mem.h"

#include "mem/frame.h"
#include "mem/page.h"
#include "mem/pool.h"
#include "aex/string.h"

#include "mem.h"

extern uint64_t frame_current, frame_last, frames_possible;
extern memfr_alloc_piece_root_t memfr_alloc_piece0;

void mem_syscall_init();

void mem_init_multiboot(multiboot_info_t* mbt) {
    printk(PRINTK_INIT "Finding memory...\n");

    mem_total_size  = 0;
    frame_current   = 0;
    frame_last      = 0;
    frames_possible = 0;

    memfr_alloc_piece0.next = 0;

    int pieces = 1;

    memfr_alloc_piece_t* piece = (memfr_alloc_piece_t*) &memfr_alloc_piece0;
    multiboot_memory_map_t* mmap = (multiboot_memory_map_t*)((phys_addr)mbt->mmap_addr);

    memset(&(memfr_alloc_piece0.bitmap), 0, sizeof(memfr_alloc_piece0.bitmap));

    uint32_t system_frame_amount = 0;
    for (uint32_t i = (phys_addr)&_start_text; i < (phys_addr)&_end_bss; i += MEM_FRAME_SIZE)
        system_frame_amount++;

    uint32_t frame_pieces_amount = 0;

	while ((phys_addr)mmap < ((phys_addr)mbt->mmap_addr + (size_t) mbt->mmap_length)) {
        if (mmap->type != MULTIBOOT_MEMORY_AVAILABLE || mmap->addr < 0x0100000) {
		    mmap = (multiboot_memory_map_t*) (phys_addr)((phys_addr)mmap + mmap->size + sizeof(mmap->size));
            continue;
        }
        if (frame_current == 0)
            frame_current = mmap->addr;

        while (frame_current < mmap->addr)
            frame_current += MEM_FRAME_SIZE;

        while (frame_current < (mmap->addr + mmap->len)) {
            if (piece->start == 0x0000) {
                piece->start = frame_current;
                frame_last   = frame_current;
            }

            if (piece == (memfr_alloc_piece_t*) &memfr_alloc_piece0 && (piece->usable == (sizeof(memfr_alloc_piece0.bitmap) * 8)))
                ;
            else if (frame_current == frame_last) {
                piece->usable++;
                goto skip;
            }

            if (piece != (memfr_alloc_piece_t*) &memfr_alloc_piece0) {
                size_t len = ((piece->usable / 8) + (CPU_PAGE_SIZE - 1));
                for (size_t i = 1; i < len / CPU_PAGE_SIZE; i++) {
                    void* ptr = (void*) kfalloc(system_frame_amount + frame_pieces_amount++);
                    memset(ptr, 0, MEM_FRAME_SIZE);
                }
            }
            memfr_alloc_piece_t* new_piece = (memfr_alloc_piece_t*) kfalloc(system_frame_amount + frame_pieces_amount++);
            pieces++;

            new_piece->start = frame_current;
            new_piece->usable = 0;
            new_piece->next = 0;

            piece->next = new_piece;
            piece = new_piece;

        skip:
            frame_current += MEM_FRAME_SIZE;
            frame_last     = frame_current;

            ++frames_possible;
        }
        mem_total_size += mmap->len;
		mmap = (multiboot_memory_map_t*) (phys_addr) ((phys_addr) mmap + mmap->size + sizeof(mmap->size));
	}
    
    if (piece->usable > 0 && (piece != (memfr_alloc_piece_t*) &memfr_alloc_piece0)) {
        size_t end_len = ((piece->usable / 8) + (CPU_PAGE_SIZE - 1));

        for (size_t i = 1; i < end_len / CPU_PAGE_SIZE; i++) {
            void* ptr = (void*) kfalloc(system_frame_amount + frame_pieces_amount++);
            memset(ptr, 0, MEM_FRAME_SIZE);
        }
    }
    printk("Total (available) size: %li KB\n", mem_total_size / 1000);
    printk("Available frames: %li\n", frames_possible);

    size_t frame_reserved = frame_pieces_amount + system_frame_amount;

    printk("Bitmap frames : %i (over %i pieces)\n", frame_pieces_amount, pieces);
    printk("System frames : %i\n", system_frame_amount);
    printk("Frame overhead: %li KB\n", (frame_reserved * CPU_PAGE_SIZE) / 1024);
    
    kfcalloc(system_frame_amount);

    mempg_init();
    mempg_init2();

    piece = (memfr_alloc_piece_t*) &memfr_alloc_piece0;
    while (true) {
        if (piece->next == NULL)
            break;

        piece->next = kpmap(kptopg(((piece->usable / 8) + (CPU_PAGE_SIZE - 1)) + 1), (phys_addr) piece->next, NULL, PAGE_WRITE);
        piece = piece->next;
    }
    mempo_init();

    mem_syscall_init();
}