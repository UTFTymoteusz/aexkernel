#pragma once

#include <stdio.h>

#include "boot/multiboot.h"
#include "dev/cpu.h"

#include "mem/frame.h"
#include "mem/page.h"
#include "mem/pool.h"

#define MEM_FRAME_SIZE CPU_PAGE_SIZE

const void * _start_text  , * _end_text;
const void * _start_rodata, * _end_rodata;
const void * _start_data  , * _end_data;
const void * _start_bss   , * _end_bss;

uint64_t mem_total_size = 0;

void mem_init_multiboot(multiboot_info_t* mbt) {
    printf("Finding memory...\n");

    mem_total_size  = 0;
    frame_current   = 0;
    frame_last      = 0;
    frames_possible = 0;

    memfr_alloc_piece0.next = 0;

    // I'm paranoid about C
    for (int i = 0; i < INTS_PER_PIECE; i++)
        memfr_alloc_piece0.bitmap[i] = 0;

    memfr_alloc_piece_t *piece = &memfr_alloc_piece0;
    multiboot_memory_map_t* mmap = (multiboot_memory_map_t*)((addr)mbt->mmap_addr);

    uint32_t system_frame_amount = 0;
    for (uint32_t i = (addr)&_start_text; i < (addr)&_end_bss; i += MEM_FRAME_SIZE)
        system_frame_amount++;

    uint32_t frame_pieces_amount = 0;

	while ((addr)mmap < ((addr)mbt->mmap_addr + (addr)mbt->mmap_length)) {
        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE && mmap->addr >= 0x0100000) {
            if (frame_current == 0)
                frame_current = mmap->addr;

            while (frame_current < mmap->addr)
                frame_current += MEM_FRAME_SIZE;

            while ((frame_current) < (mmap->addr + mmap->len)) {
                if (!piece->start) {
                    piece->start = frame_current;
                    frame_last = frame_current;
                }

                if (piece->usable < FRAMES_PER_PIECE && frame_current == frame_last)
                    piece->usable++;
                else {
                    memfr_alloc_piece_t* new_piece = (memfr_alloc_piece_t*) memfr_alloc(system_frame_amount + (frame_pieces_amount++));

                    new_piece->start = frame_current;
                    new_piece->next = 0;

                    // Still paranoid about C
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
        }
		mmap = (multiboot_memory_map_t*) (addr)( (addr)mmap + mmap->size + sizeof(mmap->size) );
	}
    printf("Total (available) size: %i KB\n", mem_total_size / 1000);
    printf("Available frames: %i\n", frames_possible);

    size_t frame_reserved = frame_pieces_amount + system_frame_amount;

    printf("Bitmap frames: %i\n", frame_pieces_amount);
    printf("System frames: %i\n", system_frame_amount);
    printf("Overhead: %i KB\n", frame_reserved * 4);
    printf("\n");

    mempg_next(system_frame_amount, NULL, NULL, 0x03);
    mempo_init();
}