#include "aex/kmem.h"
#include "aex/rcode.h"

#include "dev/dev.h"
#include "dev/block.h"
#include "dev/name.h"

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#include "part.h"

struct mbr_partition {
    uint8_t attributes;

    uint8_t chs_addr[3];
    uint8_t type;
    uint8_t chs_last_addr[3];

    uint32_t lba_start;
    uint32_t lba_count;
} __attribute((packed));
typedef struct mbr_partition mbr_partition_t;

struct mbr {
    uint8_t code[440];
    uint32_t uid;
    uint16_t reserved;

    struct mbr_partition partitions[4];
    
    uint16_t signature;
} __attribute((packed));
typedef struct mbr mbr_t;

struct dev_part {
    char* name;

    int self_dev_id;
    struct dev_block* self_dev_block;
    struct dev_block* proxy_to;

    int block_id;
    uint64_t start;
    uint64_t count;
};
typedef struct dev_part dev_part_t;

struct dev_block_ops part_block_ops;

dev_part_t* part_devs[DEV_ARRAY_SIZE];

inline int find_free_entry() {
    for (int i = 0; i < DEV_ARRAY_SIZE; i++)
        if (part_devs[i] == NULL)
            return i;
    
    return -1;
}

int fs_enum_partitions(int dev_id) {
    uint16_t flags = dev_block_get_data(dev_id)->flags;
    void* buffer   = kmalloc(512);

    if (!(flags & DISK_PARTITIONABLE))
        return ERR_NOT_POSSIBLE;

    int ret = dev_block_read(dev_id, 0, 1, buffer);
    if (ret < 0) {
        kfree(buffer);
        return ret;
    }
    mbr_t* mbr = buffer;
    mbr_partition_t* part;

    if (mbr->signature != 0xAA55) {
        kfree(buffer);
        return ERR_NOT_POSSIBLE;
    }
    char name_buffer[16];

    for (int i = 0; i < 4; i++) {
        part = &(mbr->partitions[i]);

        if (part->type == 0)
            continue;

        int id = find_free_entry();

        dev_part_t*  dev_part       = kmalloc(sizeof(dev_part_t));
        dev_block_t* dev_part_block = kmalloc(sizeof(dev_block_t));

        dev_part->self_dev_block = dev_part_block;
        dev_part->block_id = dev_id;
        dev_part->name     = kmalloc(16);

        part_devs[id] = dev_part;

        dev_id2name(dev_id, name_buffer);
        sprintf(dev_part->name, "%s%i", name_buffer, i + 1);
        
        printf("Partition %i (/dev/%s)\n", i, dev_part->name);
        printf("  Type     : 0x%x\n", part->type);
        printf("  LBA Start: %i\n",   part->lba_start);
        printf("  LBA Count: %i\n",   part->lba_count);

        dev_part_block->proxy_to     = dev_part->proxy_to;
        dev_part_block->proxy_offset = part->lba_start;

        int reg_result = dev_register_block(dev_part->name, dev_part_block);
        if (reg_result < 0) {
            printf("/dev/%s: Registration failed\n", dev_part->name);
            continue;
        }
        dev_block_set_proxy(dev_part_block, dev_block_get_data(dev_id));
        dev_part->self_dev_id = reg_result;
    }
    kfree(buffer);
    return 0;
}