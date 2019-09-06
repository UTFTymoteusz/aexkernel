#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dev/cpu.h"
#include "dev/tty.h"
#include "kernel/init.h"
#include "mem/mem.h"
#include "mem/frame.h"
#include "mem/pool.h"

#define DEFAULT_COLOR 97
#define HIGHLIGHT_COLOR 93

char stringbuffer[32];

//, size_t stack_addr
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

    interrupts();

	//mem_pool_alloc(8192);

	//asm volatile("xchg bx, bx");
	//mem_pool_enum_blocks(mem_pool0);
	//asm volatile("int 80");
}