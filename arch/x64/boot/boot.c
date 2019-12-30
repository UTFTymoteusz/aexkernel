#include "aex/dev/block.h"
#include "aex/dev/dev.h"
#include "aex/dev/name.h"

#include "boot/multiboot.h"

#include "boot.h"

bool find_boot_device(multiboot_info_t* mbt, char* rootname) {
    uint8_t boot_id = mbt->boot_device >> 24; // Take partitions into account later on
    dev_block_t* blk = NULL;

    int hdd = -1;
    if (boot_id >= 0x80 && boot_id < 0x9F)
        hdd = boot_id - 0x80;

    for (int i = 0; i < DEV_ARRAY_SIZE; i++) {
        blk = dev_block_get_data(i);
        if (blk == NULL || dev_block_is_proxy(i))
            continue;

        switch (blk->type) {
            case DISK_TYPE_DISK:
                if (hdd-- == 0) {
                    dev_id2name(i, rootname);
                    return true;
                }
                break;
            case DISK_TYPE_OPTICAL:
                if (boot_id == 0xE0 || boot_id == 0xEF || boot_id == 0x9F) {
                    dev_id2name(i, rootname);
                    return true;
                }
                break;
            default:
                break;
        }
    }
    return false;
}