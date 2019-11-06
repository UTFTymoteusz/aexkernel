#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "aex/cbuf.h"
#include "aex/cbufm.h"
#include "aex/klist.h"
#include "aex/kmem.h"
#include "aex/rcode.h"
#include "aex/time.h"

#include "boot/multiboot.h"

#include "dev/arch.h"
#include "dev/block.h"
#include "dev/cpu.h"
#include "dev/dev.h"
#include "dev/input.h"
#include "dev/name.h"
#include "dev/pci.h"
#include "dev/tty.h"

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

void test() {
    sleep(2000);
    process_debug_list();

    sleep(435345);
}

void mount_initial();

void main(multiboot_info_t* mbt) {
    cpu_init();
    tty_init_multiboot(mbt);

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
    mbt = (multiboot_info_t*) ((void*) mbt + 0xFFFFFFFF80000000);

    dev_init();

    proc_init();
    task_init();
    proc_initsys();

    tty_init_post();

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

    mount_initial(mbt);

    printf("Kernel memory: %i KiB\n", process_used_memory(1) / 1024);
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

    file_t* writer = kmalloc(sizeof(file_t));
    file_t* reader = kmalloc(sizeof(file_t));

    fs_pipe_create(reader, writer, 23);

    struct process* init = process_get(2);
    proc_set_stdin(init, tty4init_r);
    proc_set_stdout(init, writer);
    proc_set_stderr(init, writer);

    process_start(init);

    thread_t* boi = thread_create(process_current, test, true);
    thread_start(boi);

    uint8_t xd[342];

    file_t* tty0 = kmalloc(sizeof(file_t));
    fs_open("/dev/tty0", tty0);

    int len;
    while (true) {
        fs_read(reader, xd, 1);
        fs_write(tty0, xd, 1);

        len = cbuf_available(&writer->pipe->cbuf);
        if (len == 0)
            continue;

        fs_read(reader, xd, len);
        fs_write(tty0, xd, len);
    }
}

void mount_initial(multiboot_info_t* mbt) {
    uint8_t boot_id = mbt->boot_device >> 24; // Take partitions into account later on

    char rootname[24] = {'\0'};
    dev_block_t* blk = NULL;

    int hdd = -1;
    if (boot_id >= 0x80)
        hdd = boot_id - 0x80;

    for (int i = 0; i < DEV_ARRAY_SIZE; i++) {
        blk = dev_block_get_data(i);
        if (blk == NULL || dev_block_is_proxy(i))
            continue;

        switch (blk->type) {
            case DISK_TYPE_DISK:
                if (hdd-- == 0) {
                    dev_id2name(i, rootname);
                    goto found_boot;
                }
                break;
            case DISK_TYPE_OPTICAL:
                if (boot_id == 0xE0) {
                    dev_id2name(i, rootname);
                    goto found_boot;
                }
                break;
            default:
                break;
        }
    }
found_boot:
    if (strlen(rootname) == 0)
        kpanic("Failed to find boot device");

    printf("Apparently we've been booted from /dev/%s\n", rootname);
    int mnt_res;
    
    mnt_res = fs_mount(rootname, "/", NULL);
    if (mnt_res < 0)
        kpanic("Failed to mount the root device");

    mnt_res = fs_mount(NULL, "/dev/", "devfs");
    if (mnt_res < 0)
        kpanic("Failed to mount the devfs");
}