#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


#include "dev/cpu.h"
#include "dev/dev.h"
#include "dev/pci.h"
#include "dev/tty.h"

#include "drv/ata/ahci.h"
#include "drv/ttyk/ttyk.h"

#include "kernel/init.h"
#include "kernel/syscall.h"

#include "mem/mem.h"
#include "mem/frame.h"
#include "mem/pool.h"

#include "proc/proc.h"
#include "proc/task.h"

#include "aex/time.h"

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
	printf(".text  : 0x%x, 0x%x\n", (long)&_start_text, (long)&_end_text);
	printf(".rodata: 0x%x, 0x%x\n", (long)&_start_rodata, (long)&_end_rodata);
	printf(".data  : 0x%x, 0x%x\n", (long)&_start_data, (long)&_end_data);
	printf(".bss   : 0x%x, 0x%x\n", (long)&_start_bss, (long)&_end_bss);
	printf("\n");

    mem_init_multiboot(mbt);

	dev_init();

	proc_init();
    task_init();

	pci_init();

	// Devices
	ttyk_init();
	ahci_init();
	//ata_init();

	//int ttyk_id = dev_name2id("ttyk");

	//dev_open(ttyk_id);
	//dev_write(ttyk_id, "hello\n", 6);

    interrupts();
	
	//printf("  'Direct' 0x%s\n", itoa(((size_t*)0x100000)[0], stringbuffer, 16));
	
	//page_remove((void*)0xFFFFFFFF80100000, NULL);
	
	//page_assign((void*)0xFFFFFFFF80100000, (void*)0x100000, NULL, 0b011);
	//printf("  Mapped   0x%s\n", itoa(((size_t*)0xFFFFFFFF80100000)[0], stringbuffer, 16));

	/*{
		size_t i = 0;
		struct fs_descriptor* desc = fs_mounts[i];

		printf("Registered Filesystems\n");

		while (desc != NULL) {
			
			printf(" - %s\n", desc->name);

			desc = fs_mounts[++i];
		}
	}*/

	//mempo_enum(mem_pool0);
	while (true) {
		printf("Kernel loop (15s)\n");
		//printf("AAAAA\n");

		syscall_sleep(15000);
	}
}