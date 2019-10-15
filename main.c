#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "aex/klist.h"
#include "aex/rcode.h"
#include "aex/time.h"

#include "boot/multiboot.h"

#include "dev/cpu.h"
#include "dev/dev.h"
#include "dev/tty.h"
#include "dev/pci.h"

#include "drv/ata/ahci.h"
#include "drv/ttyk/ttyk.h"

#include "fs/fs.h"
#include "fs/drv/devfs/devfs.h"
#include "fs/drv/fat/fat.h"
#include "fs/drv/iso9660/iso9660.h"

#include "kernel/init.h"
#include "kernel/sys.h"

#include "mem/mem.h"

#include "proc/proc.h"
#include "proc/task.h"

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
    proc_initsys();

    pci_init();

    // Devices
    ttyk_init();
    ahci_init();

    fs_init();
    fat_init();
    iso9660_init();
    devfs_init();

    interrupts();

    fs_mount("sra", "/", NULL);
    fs_mount(NULL, "/dev/", "devfs");

    printf("Starting ");
    tty_set_color_ansi(93);
    printf("/sys/aexinit.elf\n");
    tty_set_color_ansi(97);

    int init_c_res = process_icreate("/sys/aexinit.elf");
    if (init_c_res == FS_ERR_NOT_FOUND)
        kpanic("/sys/aexinit.elf not found");
    else if (init_c_res < 0)
        kpanic("Failed to start /sys/aexinit.elf");

    process_debug_list();

    struct process* boi1 = process_get(1);
    printf("Kernel: Dir pages: %i, Data pages: %i : (%i KiB)\n", boi1->ptracker->dir_frames_used, boi1->ptracker->frames_used, (boi1->ptracker->dir_frames_used + boi1->ptracker->frames_used) * 4);
    
    struct process* boi2 = process_get(2);
    printf("Init:   Dir pages: %i, Data pages: %i : (%i KiB)\n", boi2->ptracker->dir_frames_used, boi2->ptracker->frames_used, (boi2->ptracker->dir_frames_used + boi2->ptracker->frames_used) * 4);

    while (true) {
        printf("interleaving\n");
        sleep(2000);
    }
}