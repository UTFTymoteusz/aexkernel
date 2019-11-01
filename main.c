#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "aex/cbufm.h"
#include "aex/klist.h"
#include "aex/kmem.h"
#include "aex/rcode.h"
#include "aex/time.h"

#include "boot/multiboot.h"

#include "dev/arch.h"
#include "dev/cpu.h"
#include "dev/dev.h"
#include "dev/input.h"
#include "dev/tty.h"
#include "dev/pci.h"

#include "drv/ata/ahci.h"
#include "drv/ata/ata.h"
#include "drv/ttyk/ttyk.h"

#include "fs/fs.h"
#include "fs/file.h"
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

    input_init();

    pci_init();
    arch_init();

    // Devices
    ttyk_init();
    ahci_init();
    //ata_init();

    fs_init();
    fat_init();
    iso9660_init();
    devfs_init();

    interrupts();

    fs_mount("sdc", "/", NULL);
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

    file_t* tty4init_r = kmalloc(sizeof(file_t));
    fs_open("/dev/tty0", tty4init_r);
    tty4init_r->flags |= FILE_FLAG_READ;

    file_t* tty4init_w = kmalloc(sizeof(file_t));
    fs_open("/dev/tty0", tty4init_w);
    tty4init_w->flags |= FILE_FLAG_WRITE;

    struct process* init = process_get(2);
    proc_set_stdin(init, tty4init_r);
    proc_set_stdout(init, tty4init_w);
    proc_set_stderr(init, tty4init_w);

    process_start(init);

    while (true) {
        sleep(5000);
    }
}