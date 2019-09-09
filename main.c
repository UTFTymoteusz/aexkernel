#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dev/cpu.h"
#include "dev/tty.h"
#include "kernel/init.h"
#include "kernel/syscall.h"
#include "mem/mem.h"
#include "mem/frame.h"
#include "mem/pool.h"
#include "proc/task.h"

#define DEFAULT_COLOR 97
#define HIGHLIGHT_COLOR 93

char stringbuffer[32];

void main(multiboot_info_t* mbt) {

	cpu_init();
    tty_init();

	init_print_header();
	tty_set_color_ansi(DEFAULT_COLOR);

	init_print_osinfo();

	printf("Section info:\n");
	printf(".text  : 0x%s, ", itoa((long)&_start_text, stringbuffer, 16));
	printf("0x%s\n", itoa((long)&_end_text, stringbuffer, 16));
	printf(".rodata: 0x%s, ", itoa((long)&_start_rodata, stringbuffer, 16));
	printf("0x%s\n", itoa((long)&_end_rodata, stringbuffer, 16));
	printf(".data  : 0x%s, ", itoa((long)&_start_data, stringbuffer, 16));
	printf("0x%s\n", itoa((long)&_end_data, stringbuffer, 16));
	printf(".bss   : 0x%s, ", itoa((long)&_start_bss,  stringbuffer, 16));
	printf("0x%s\n", itoa((long)&_end_bss, stringbuffer, 16));
	printf("\n");

    mem_init_multiboot(mbt);
	
	syscall_init();

	task_init();

	//asm volatile("xchg bx, bx");
    interrupts();
	
	//printf("  'Direct' 0x%s\n", itoa(((size_t*)0x100000)[0], stringbuffer, 16));
	
	//page_remove((void*)0xFFFFFFFF80100000, NULL);
	
	//page_assign((void*)0xFFFFFFFF80100000, (void*)0x100000, NULL, 0b011);
	//printf("  Mapped   0x%s\n", itoa(((size_t*)0xFFFFFFFF80100000)[0], stringbuffer, 16));

	//mem_pool_enum_blocks(mem_pool0);

	while (true) {
		printf("Kernel loop\n");

		for (size_t i = 0; i < 100; i++)
			waitforinterrupt();
	}
}