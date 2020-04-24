#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aex/aex.h"
#include "aex/cbuf.h"
#include "aex/cbufm.h"
#include "aex/debug.h"
#include "aex/hook.h"
#include "aex/kernel.h"
#include "aex/klist.h"
#include "aex/mem.h"
#include "aex/rcode.h"
#include "aex/string.h"

#include "aex/dev/dev.h"
#include "aex/dev/block.h"
#include "aex/dev/name.h"
#include "aex/dev/tty.h"

#include "aex/fs/fd.h"
#include "aex/fs/fs.h"

#include "aex/proc/task.h"

#include "aex/sys/irq.h"
#include "aex/sys/sys.h"
#include "aex/sys/time.h"

#include "boot/boot.h"
#include "boot/multiboot.h"

#include "dev/arch.h"

#include "drv/ata/ahci.h"
//#include "drv/ata/ata.h"
#include "drv/pseudo/pseudo.h"
#include "drv/ttyk/ttyk.h"

#include "kernel/acpi/acpi.h"
#include "kernel/init.h"

#include "mem/frame.h"
#include "mem/mem.h"

// static, local stuff
#include "dev/test.h"

/*void test() {
    sleep(2000);
    while (true) {
        printk("Used frames: %li; Mapped memory: %li KiB\n", kfused(), process_mapped_memory(KERNEL_PROCESS) / 1024);
        process_debug_list();
        //printk("0x%p\n", thread_get(KERNEL_PROCESS, 0)->task);
        sleep(5000);
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
    sleep(2000);
}*/

void mount_initial();
void run_user_test(char* path);

void* async_init_test(void* args) {
    task_tsleep(1000);
    task_prkill(3, 0);

    return NULL;
}

void timertest() {
    while (true) {
        task_tsleep(1000);
        printk("second\n");
    }
}

void kernel_main(multiboot_info_t* mbt) {
    cpu_init();

    tty_init();
    tty_clear(0);

    //tty_init_multiboot(mbt);

    init_print_header();
    tty_set_color_ansi(0, DEFAULT_COLOR);

    init_print_osinfo();

    printk("Section info:\n");
    printk(".text  : %${93}0x%p%${97}, %${93}0x%p%${97}\n", &_start_text,   &_end_text);
    printk(".rodata: %${93}0x%p%${97}, %${93}0x%p%${97}\n", &_start_rodata, &_end_rodata);
    printk(".data  : %${93}0x%p%${97}, %${93}0x%p%${97}\n", &_start_data,   &_end_data);
    printk(".bss   : %${93}0x%p%${97}, %${93}0x%p%${97}\n", &_start_bss,    &_end_bss);
    printk("\n");

    mem_init_multiboot(mbt);
    mbt = (multiboot_info_t*) kpmap(kptopg(sizeof(multiboot_info_t)), (phys_addr) mbt, NULL, PAGE_WRITE);
    dev_init();

    tty_init_post();

    task_init();

    irq_initsys();

    acpi_init();

    interrupts();

    input_init();
    sys_init();

    pci_init();
    arch_init();
    printk(PRINTK_OK "Arch-specifics initialized\n\n");

    // Devices
    pseudo_init();
    ttyk_init();
    printk("\n");

    ahci_init();

    //ata_init();
    printk(PRINTK_OK "Storage drivers initialized\n\n");

    fs_init();

    mount_initial(mbt);
    printk("\n");

    debug_load_symbols();
    printk(PRINTK_OK "Debug kernel symbols loaded\n");
    printk("\n");

    process_info_t pinfo;
    task_prinfo(KERNEL_PROCESS, &pinfo);

    printk("Kernel memory: %li (+ %li) KiB\n", pinfo.used_phys_memory / 1024, 
                                                pinfo.mapped_memory / 1024);
    printk("Starting %${93}/sys/aexinit.elf%${97}\n");
    printk("Used frames: %li\n", kfused());

    char* init_args[] = {"/sys/aexinit.elf", NULL};

    int tty0_r = fs_open("/dev/tty0", FS_MODE_READ);
    int tty0_w = fs_open("/dev/tty0", FS_MODE_WRITE);
    int tty0_W = fd_dup(tty0_w);

    pcreate_info_t info = {
        .stdin  = tty0_r,
        .stdout = tty0_w,
        .stderr = tty0_W,
        .cwd = "/",
    };

    pid_t init_c_res = task_pricreate("/sys/aexinit.elf", init_args, &info);
    if (init_c_res == -ENOENT)
        kpanic("/sys/aexinit.elf not found");
    else if (init_c_res < 0)
        kpanic("Failed to start /sys/aexinit.elf");

    task_prstart(INIT_PROCESS);
 
    /*hook_add(HOOK_PSTART, "root_pstart_test", pstart_hook_test);
    hook_add(HOOK_PKILL , "root_pkill_test" , pkill_hook_test );
    hook_add(HOOK_USR_FACCESS, "root_usr_fopen_test", usr_faccess_hook_test);
    hook_add(HOOK_SHUTDOWN   , "root_shutdown_test" , shutdown_hook_test);*/

    testdev_init();
    task_tsleep(500);

    run_user_test("/sys/test/test00.elf");
    run_user_test("/sys/test/test01.elf");

    int ecode;

    task_prwait(INIT_PROCESS, &ecode);
    printk("aexinit exited, shutting down\n");

    task_tsleep(1500);

    shutdown();
    halt();
}

struct ttysize {
    uint16_t rows;
    uint16_t columns;
    uint16_t pixel_height;
    uint16_t pixel_width;
};

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
        kpanic("Failed to mount devfs");

    /*
    int ret;
    dentry_t dentry;

    int fdd = fs_opendir("/sys/");
    ret = 0;

    while (true) {
        ret = fd_readdir(fdd, &dentry);
        if (ret == -1)
            break;
            
        printk("%s; ", dentry.name);

        //task_tsleep(200);
    }
    ret = fd_close(fdd);

    int fd = fs_open("/test.txt", 0);

    ret = fd_seek(fd, -64, 2);

    char buff[65] = {0};
    ret = fd_read(fd, buff, 64);

    printk("ret: %i\n", ret);
    printk("buff: %s\n", (uint8_t*) buff);

    ret = fd_close(fd);

    finfo_t boi;
    ret = fs_finfo("/sys/aexkrnl.elf", &boi);
    printk("ret: %i\n", ret);
    printk("size: %lu\n", boi.size);

    int fd2 = fs_open("/dev/tty0", 0);
    fd_write(fd2, "boi\n", 4);
    fd_close(fd2);
    */
}
/*
void print_filesystems() {
    klist_entry_t* klist_entry = NULL;
    struct filesystem* entry = NULL;

    while (true) {
        entry = (struct filesystem*) klist_iter(&filesystems, &klist_entry);
        if (entry == NULL)
            break;

        printk(PRINTK_BARE " - %s\n", entry->name);
    }
}*/

void run_user_test(char* path) {
    printk(PRINTK_WARN "Begin test %s\n", path);

    char* test_args[] = {path, NULL};

    int ttty_0 = fs_open("/dev/tty0", FS_MODE_READ | FS_MODE_WRITE);
    int ttty_1 = fd_dup(ttty_0);
    int ttty_2 = fd_dup(ttty_0);

    pcreate_info_t pinfo = {
        .stdin  = ttty_0,
        .stdout = ttty_1,
        .stderr = ttty_2,
        .cwd = "/sys/test/",
    };

    int test_c_res = task_pricreate(path, test_args, &pinfo);
    if (test_c_res == -ENOENT)
        printk("%s not found", path);
    else if (test_c_res < 0)
        printk("Failed to start %s", path);

    task_prstart(test_c_res);

    int rcode = 0;
    task_prwait(test_c_res, &rcode);

    if (rcode < 0)
        kpanic("User test failed");

    fd_close(ttty_0);
    fd_close(ttty_1);
    fd_close(ttty_2);
}