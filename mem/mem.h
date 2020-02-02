#pragma once

#include "boot/multiboot.h"

#define MEM_FRAME_SIZE CPU_PAGE_SIZE

const void* _start_text  ,* _end_text;
const void* _start_rodata,* _end_rodata;
const void* _start_data  ,* _end_data;
const void* _start_bss   ,* _end_bss;

uint64_t mem_total_size;

void mem_init_multiboot(multiboot_info_t* mbt);