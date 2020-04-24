#include "aex/rcparray.h"

#include "aex/dev/class/disk.h"

#include "aex/dev/block.h"
#include "aex/dev/dev.h"
#include "aex/dev/name.h"

#include "boot/multiboot.h"

#include "boot.h"

extern rcparray(dev_t*) dev_array;

bool find_boot_device(multiboot_info_t* mbt, char* rootname) {
    uint8_t boot_id = mbt->boot_device >> 24; // Take partitions into account later on
    dev_block_t* blk = NULL;

    int hdd = -1;
    if (boot_id >= 0x80 && boot_id < 0x9F)
        hdd = boot_id - 0x80;

    int id = 0;
    rcparray_foreach(dev_array, id) {
        blk = dev_block_get(id);
        if (blk == NULL)
            continue;

        dev_id2name(id, rootname);
        printk("%s  ", rootname);

        switch (disk_get_type(rootname)) {
            case DISK_TYPE_DISK:
                dev_block_unref(id);
                if (hdd-- == 0)
                    return true;

                break;
            case DISK_TYPE_OPTICAL:
                dev_block_unref(id);
                if (boot_id == 0xE0 || boot_id == 0xEF || boot_id == 0x9F)
                    return true;

                break;
            default:
                dev_block_unref(id);
                break;
        }
    }
    return false;
}