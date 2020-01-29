#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aex/aex.h"
#include "aex/cbuf.h"
#include "aex/cbufm.h"
#include "aex/debug.h"
#include "aex/hook.h"
#include "aex/irq.h"
#include "aex/kernel.h"
#include "aex/klist.h"
#include "aex/mem.h"
#include "aex/rcode.h"
#include "aex/string.h"
#include "aex/sys.h"
#include "aex/time.h"

#include "aex/dev/dev.h"
#include "aex/dev/block.h"
#include "aex/dev/name.h"
#include "aex/dev/tty.h"

#include "aex/fs/fs.h"
#include "aex/fs/file.h"

#include "aex/proc/proc.h"
#include "aex/proc/task.h"

#include "boot/boot.h"
#include "boot/multiboot.h"

#include "dev/arch.h"

#include "drv/ata/ahci.h"
#include "drv/ata/ata.h"
#include "drv/pseudo/pseudo.h"
#include "drv/ttyk/ttyk.h"

#include "fs/drv/devfs/devfs.h"
#include "fs/drv/fat/fat.h"
#include "fs/drv/iso9660/iso9660.h"

#include "kernel/acpi.h"
#include "kernel/init.h"

#include "mem/frame.h"
#include "mem/mem.h"

void test() {
    sleep(2000);
    while (true) {
        printk("Used frames: %li; Mapped memory: %li KiB\n", kfused(), process_mapped_memory(KERNEL_PROCESS) / 1024);
        //process_debug_list();
        sleep(2500);
    }
}

void pstart_hook_test(hook_proc_data_t* data) {
    printk("Process %li got started\n", data->pid);
}

void pkill_hook_test(hook_proc_data_t* data) {
    printk("Process %li got game-ended\n", data->pid);
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

    printk("Access check for '%s': %s by PID %li\n", data->path, boi, data->pid);
    return 0;
}

void shutdown_hook_test(UNUSED hook_proc_data_t* data) {
    printk("\nOh, a shutdown\n");
    sleep(1000);
}

void mount_initial();
void print_filesystems();

void kernel_main(multiboot_info_t* mbt) {
    set_printk_flags(0);

    cpu_init();
    tty_init_multiboot(mbt);

    init_print_header();
    tty_set_color_ansi(DEFAULT_COLOR);

    init_print_osinfo();

    printk("Section info:\n");
    printk(".text  : %${93}0x%p%${97}, %${93}0x%p%${97}\n", &_start_text,   &_end_text);
    printk(".rodata: %${93}0x%p%${97}, %${93}0x%p%${97}\n", &_start_rodata, &_end_rodata);
    printk(".data  : %${93}0x%p%${97}, %${93}0x%p%${97}\n", &_start_data,   &_end_data);
    printk(".bss   : %${93}0x%p%${97}, %${93}0x%p%${97}\n", &_start_bss,    &_end_bss);
    printk("\n");

    mem_init_multiboot(mbt);
    mbt = (multiboot_info_t*) kpmap(kptopg(sizeof(multiboot_info_t)), (phys_addr) mbt, NULL, PAGE_WRITE);
    //set_printk_flags(PRINTK_TIME);
    dev_init();

    tty_init_post();

    proc_init();
    task_init();
    proc_initsys();

    irq_initsys();
    interrupts();

    input_init();
    sys_init();

    pci_init();
    arch_init();
    printk(PRINTK_OK "Arch-specifics initialized\n\n");

    acpi_init();

    // Devices
    pseudo_init();
    ttyk_init();
    printk("\n");

    ahci_init();
    //ata_init();
    printk(PRINTK_OK "Storage drivers initialized\n\n");

    fs_init();
    
    printk(PRINTK_INIT "fs: Registering initial filesystems...\n");
    fat_init();
    iso9660_init();
    devfs_init();

    printk("fs: Registered filesystems:\n");
    print_filesystems();

    mount_initial(mbt);
    printk("\n");

    //tid_t test_id = thread_create(KERNEL_PROCESS, test, true);
    //thread_start(KERNEL_PROCESS, test_id);

    printk("Kernel memory: %li (+ %li) KiB\n", process_used_phys_memory(KERNEL_PROCESS) / 1024, process_mapped_memory(KERNEL_PROCESS) / 1024);
    printk("Starting %${93}/sys/aexinit.elf%${97}\n");
    printk("Used frames: %li\n", kfused());

    char* init_args[] = {"/sys/aexinit.elf", NULL};

    int init_c_res = process_icreate("/sys/aexinit.elf", init_args);
    if (init_c_res == ERR_NOT_FOUND)
        kpanic("/sys/aexinit.elf not found");
    else if (init_c_res < 0)
        kpanic("Failed to start /sys/aexinit.elf");

    file_t* tty4init_r = kmalloc(sizeof(file_t));
    fs_open("/dev/tty0", tty4init_r);
    tty4init_r->flags |= FILE_FLAG_READ;

    file_t* tty4init_w = kmalloc(sizeof(file_t));
    fs_open("/dev/tty0", tty4init_w);
    tty4init_w->flags |= FILE_FLAG_WRITE;

    hook_add(HOOK_PSTART, "root_pstart_test", pstart_hook_test);
    hook_add(HOOK_PKILL , "root_pkill_test" , pkill_hook_test );
    hook_add(HOOK_USR_FACCESS, "root_usr_fopen_test", usr_faccess_hook_test);
    hook_add(HOOK_SHUTDOWN   , "root_shutdown_test" , shutdown_hook_test);

    proc_set_stdin (INIT_PROCESS, tty4init_r);
    proc_set_stdout(INIT_PROCESS, tty4init_w);
    proc_set_stderr(INIT_PROCESS, tty4init_w);
    proc_set_dir(INIT_PROCESS, "/");

    set_printk_flags(0);
    process_start(INIT_PROCESS);

    io_block(&(process_get(INIT_PROCESS)->wait_list));
    printk("aexinit exitted, shutting down");

    shutdown();
}

void mount_initial(multiboot_info_t* mbt) {
    char rootname[32];
    if (!find_boot_device(mbt, rootname))
        kpanic("Failed to find boot device");

    printk("Apparently we've booted from /dev/%s\n", rootname);
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
        entry = (struct filesystem*) klist_iter(&filesystems, &klist_entry);
        if (entry == NULL)
            break;

        printk(PRINTK_BARE " - %s\n", entry->name);
    }
}