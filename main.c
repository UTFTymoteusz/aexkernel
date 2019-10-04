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

#include "fs/fs.h"
#include "fs/drv/fat/fat.h"
#include "fs/drv/iso9660/iso9660.h"

#include "kernel/init.h"
#include "kernel/syscall.h"

#include "mem/mem.h"
#include "mem/frame.h"
#include "mem/pool.h"

#include "proc/proc.h"
#include "proc/task.h"

#include "aex/byteswap.h"
#include "aex/time.h"

#define DEFAULT_COLOR 97
#define HIGHLIGHT_COLOR 93

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

	fs_init();
	fat_init();
	iso9660_init();

	//int ttyk_id = dev_name2id("ttyk");

	//dev_open(ttyk_id);
	//dev_write(ttyk_id, "hello\n", 6);

    interrupts();

	int dev_amnt = dev_current_amount();

	dev_t** devs = kmalloc(dev_amnt * sizeof(dev_t*));
	dev_list(devs);

	/*printf("Device list:\n");
	for (int i = 0; i < dev_amnt; i++)
		printf("  /dev/%s\n", devs[i]->name);*/

	fs_mount("sra", "/", NULL);
	//fs_mount("sda1", "/bigbong/", NULL);

	{
		//char* path = "/boot/grub/i386-pc/";
		char* path = "/";

		int count = fs_count(path);

		//printf("Dir count: %i\n", count);
		dentry_t* entries = kmalloc(sizeof(dentry_t) * count);

		fs_list(path, entries, count);

		for (int i = 0; i < count; i++) {
			printf("%s", entries[i].name);

			if (entries[i].type == FS_RECORD_TYPE_DIR)
				printf("/");

			printf(" - inode:%i", entries[i].inode_id);

			printf("\n");
			//sleep(2);
		}
	}
	{
		char* path = "/sys/aexkrnl.elf";
		file_t* file = kmalloc(sizeof(file_t));

		int ret = fs_open(path, file);
		//printf("Ret: %x\n", ret);

		if (ret == FS_ERR_NOT_FOUND)
			printf("Not found\n");
		else if (ret == FS_ERR_IS_DIR)
			printf("Is a directory\n");

		fs_close(file);
	}

	while (true) {
		printf("Kernel loop (45s)\n");
		//printf("AAAAA\n");

		syscall_sleep(45000);
	}
}