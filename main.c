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

#include "kernel/hook.h"
#include "kernel/init.h"
#include "kernel/irq.h"
#include "kernel/sys.h"

#include "mem/frame.h"
#include "mem/mem.h"

#include "proc/proc.h"
#include "proc/task.h"

void test() {
    sleep(2000);
    while (true) {
        printf("Used frames: %i\n", memfr_used());
        process_debug_list();
        task_debug();
        sleep(5000);
    }
}

void pstart_hook_test(hook_proc_data_t* data) {
    printf("Process %i got started\n", data->pid);
}

void pkill_hook_test(hook_proc_data_t* data) {
    printf("Process %i got game-ended\n", data->pid);
}

long usr_faccess_hook_test(hook_file_data_t* data) {
    char boi[4];

    for (int i = 0; i < 3; i++)
        boi[i] = '-';

    boi[3] = '\0';

    if (data->mode & FS_MODE_READ)
        boi[0] = 'r';
    if (data->mode & FS_MODE_WRITE)
        boi[1] = 'w';
    if (data->mode & FS_MODE_EXECUTE)
        boi[2] = 'x';

    printf("Access check for '%s': %s\n", data->path, boi);
    return 0;
}

void mount_initial();
void print_filesystems();

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

    interrupts();
    irq_initsys();

    tty_init_post();

    input_init();

    pci_init();
    arch_init();

    // Devices
    ttyk_init();
    ahci_init();
    //ata_init();

    fs_init();
    
    printf("fs: Registering initial filesystems...\n");
    fat_init();
    iso9660_init();
    devfs_init();

    printf("fs: Registered filesystems:\n");
    print_filesystems();

    mount_initial(mbt);

    printf("Kernel memory: %i (+ %i) KiB\n", process_used_phys_memory(1) / 1024, process_mapped_memory(1) / 1024);
    printf("Starting ");
    tty_set_color_ansi(93);
    printf("/sys/aexinit.elf\n");
    tty_set_color_ansi(97);
    
    printf("Used frames: %i\n", memfr_used());

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

    //thread_t* boi = thread_create(process_current, test, true);
    //thread_start(boi);

    hook_add(HOOK_PSTART, "root_pstart_test", pstart_hook_test);
    hook_add(HOOK_PKILL, "root_pkill_test", pkill_hook_test);
    hook_add(HOOK_USR_FACCESS, "root_usr_fopen_test", usr_faccess_hook_test);

    process_t* init = process_get(2);
    proc_set_stdin(init, tty4init_r);
    proc_set_stdout(init, tty4init_w);
    proc_set_stderr(init, tty4init_w);
    proc_set_dir(init, "/");

    process_start(init);

    while (true) {
        sleep(1000);
        //printf("\n<xd xd>\n");
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

    printf("Apparently we've booted from /dev/%s\n", rootname);
    int mnt_res;
    
    mnt_res = fs_mount(rootname, "/", NULL);
    if (mnt_res < 0)
        kpanic("Failed to mount the root device");

    mnt_res = fs_mount(NULL, "/dev/", "devfs");
    if (mnt_res < 0)
        kpanic("Failed to mount the devfs");
}

void print_filesystems() {
    klist_entry_t* klist_entry = NULL;
    struct filesystem* entry = NULL;

    while (true) {
        entry = (struct filesystem*)klist_iter(&filesystems, &klist_entry);
        if (entry == NULL)
            break;

        printf(" - %s\n", entry->name);
    }
}