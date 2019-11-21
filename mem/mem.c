#include "mem/frame.h"
#include "mem/page.h"
#include "mem/pool.h"

#include <stdio.h>

#include "mem.h"

extern uint64_t frame_current, frame_last, frames_possible;
extern memfr_alloc_piece_t memfr_alloc_piece0;

void mem_init_multiboot(multiboot_info_t* mbt) {
    printf("Finding memory...\n");

    mem_total_size  = 0;
    frame_current   = 0;
    frame_last      = 0;
    frames_possible = 0;

    memfr_alloc_piece0.next = 0;

    for (int i = 0; i < INTS_PER_PIECE; i++)
        memfr_alloc_piece0.bitmap[i] = 0;

    memfr_alloc_piece_t *piece = &memfr_alloc_piece0;
    multiboot_memory_map_t* mmap = (multiboot_memory_map_t*)((cpu_addr)mbt->mmap_addr);

    uint32_t system_frame_amount = 0;
    for (uint32_t i = (cpu_addr)&_start_text; i < (cpu_addr)&_end_bss; i += MEM_FRAME_SIZE)
        system_frame_amount++;

    uint32_t frame_pieces_amount = 0;

	while ((cpu_addr)mmap < ((cpu_addr)mbt->mmap_addr + (size_t) mbt->mmap_length)) {
        if (mmap->type != MULTIBOOT_MEMORY_AVAILABLE || mmap->addr < 0x0100000) {
		    mmap = (multiboot_memory_map_t*) (cpu_addr)((cpu_addr)mmap + mmap->size + sizeof(mmap->size));
            continue;
        }
        if (frame_current == 0)
            frame_current = mmap->addr;

        while (frame_current < mmap->addr)
            frame_current += MEM_FRAME_SIZE;

        while (frame_current < (mmap->addr + mmap->len)) {
            if (!piece->start) {
                piece->start = frame_current;
                frame_last = frame_current;
            }
            if (piece->usable < FRAMES_PER_PIECE && frame_current == frame_last)
                piece->usable++;
            else {
                memfr_alloc_piece_t* new_piece = (memfr_alloc_piece_t*) memfr_alloc(system_frame_amount + frame_pieces_amount++);
                for (size_t i = 1; i < (sizeof(memfr_alloc_piece_t) + (CPU_PAGE_SIZE - 1)) / CPU_PAGE_SIZE; i++)
                    memfr_alloc(system_frame_amount + frame_pieces_amount++);

                new_piece->start = frame_current;
                new_piece->next = 0;

                for (int i = 0; i < INTS_PER_PIECE; i++)
                    new_piece->bitmap[i] = 0;

                piece->next = new_piece;
                piece = new_piece;
            }
            frame_current += MEM_FRAME_SIZE;
            frame_last = frame_current;
            ++frames_possible;
        }
        mem_total_size += mmap->len;
		mmap = (multiboot_memory_map_t*) (cpu_addr)((cpu_addr)mmap + mmap->size + sizeof(mmap->size));
	}
    printf("Total (available) size: %i KB\n", mem_total_size / 1000);
    printf("Available frames: %i\n", frames_possible);

    size_t frame_reserved = frame_pieces_amount + system_frame_amount;

    printf("Bitmap frames: %i\n", frame_pieces_amount);
    printf("System frames: %i\n", system_frame_amount);
    printf("Frame overhead: %i KB\n", frame_reserved * 4);
    printf("\n");

    mempg_init();
    mempg_alloc(system_frame_amount, NULL, 0x03);
    mempo_init();

    for (cpu_addr i = 0; i < 512; i++)
        mempg_assign((void*) (i * MEM_FRAME_SIZE), (void*) (i * MEM_FRAME_SIZE), NULL, 0x03);
    
    mempg_init2();
}