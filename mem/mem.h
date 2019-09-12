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
	char stringbuffer[32];

    mem_total_size = 0;
    frame_current = 0;
    frame_last = 0;
    frames_possible = 0;

    mem_frame_alloc_piece0.next = 0;

    // I'm paranoid about C
    for (int i = 0; i < INTS_PER_PIECE; i++)
        mem_frame_alloc_piece0.bitmap[i] = 0;

    mem_frame_alloc_piece *piece = &mem_frame_alloc_piece0;

    multiboot_memory_map_t* mmap = (multiboot_memory_map_t*)((addr)mbt->mmap_addr);

    //printf(" %s \n", itoa(sizeof(mem_frame_alloc_piece), stringbuffer, 10));

    uint32_t system_frame_amount = 0;
    for (uint32_t i = (addr)&_start_text; i < (addr)&_end_bss; i += MEM_FRAME_SIZE)
        system_frame_amount++;
    
    uint32_t frame_pieces_amount = 0;
    
	while ((addr)mmap < ((addr)mbt->mmap_addr + (addr)mbt->mmap_length)) {
		
        //printf("Piece: 0x%s", itoa(mmap->addr, stringbuffer, 16));
        //printf(", %s ", itoa(mmap->len, stringbuffer, 10));

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
                    mem_frame_alloc_piece* new_piece = (mem_frame_alloc_piece*) mem_frame_alloc(system_frame_amount + (frame_pieces_amount++));

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

        /*switch (mmap->type) {
            case MULTIBOOT_MEMORY_AVAILABLE:
                printf("A");
                break;
            case MULTIBOOT_MEMORY_RESERVED:
                printf("R");
                break;
            case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
                printf("H");
                break;
            case MULTIBOOT_MEMORY_NVS:
                printf("N");
                break;
            case MULTIBOOT_MEMORY_BADRAM:
                printf("B");
                break;
        }
        printf("\n");*/

		mmap = (multiboot_memory_map_t*) (addr)( (addr)mmap + mmap->size + sizeof(mmap->size) );
	}
    printf("Total (available) size: %s KB\n", itoa(mem_total_size / 1000, stringbuffer, 10));
    printf("Available frames: %s\n", itoa(frames_possible, stringbuffer, 10));
    //printf("First alloc piece address: 0x%s\n", itoa((addr)&mem_frame_alloc_piece0, stringbuffer, 16));
    //printf("Usable frames: %s\n", itoa(mem_frame_alloc_piece0.usable, stringbuffer, 10));

    //asm volatile("xchg bx, bx");

    //asm volatile("xchg bx, bx");

    //printf("Free frame id: %s\n", itoa(memory_frame_get_free(), stringbuffer, 10));
    //printf("Frame 32607 addr: 0x%s\n", itoa((addr)mem_frame_alloc(32607), stringbuffer, 16));

    size_t frame_reserved = frame_pieces_amount + system_frame_amount;

    printf("Bitmap frames: %s\n", itoa(frame_pieces_amount, stringbuffer, 10));
    printf("System frames: %s\n", itoa(system_frame_amount, stringbuffer, 10));
    printf("Overhead: %s KB\n", itoa(frame_reserved * 4, stringbuffer, 10));
    printf("\n");

    for (uint8_t i = 0; i < system_frame_amount; i++)
        mem_page_next(NULL, NULL, 0x03);

    mem_pool_init();
}

