#pragma once

#include "dev/dev.h"
#include "dev/disk.h"
#include "dev/name.h"

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
    struct dev_disk* self_dev_disk;
    struct dev_disk* proxy_to;

    int disk_id;
    uint64_t start;
    uint64_t count;
};
typedef struct dev_part dev_part_t;

struct dev_disk_ops part_disk_ops;

dev_part_t* part_devs[DEV_ARRAY_SIZE];

inline int find_free_entry() {
    for (int i = 0; i < DEV_ARRAY_SIZE; i++)
        if (part_devs[i] == NULL)
            return i;
    
    return -1;
}

int fs_enum_partitions(int dev_id) {
    uint16_t flags = dev_disk_get_data(dev_id)->flags;
    void* buffer   = kmalloc(512);

    if (!(flags & DISK_PARTITIONABLE))
        return ERR_NOT_POSSIBLE;

    int ret = dev_disk_read(dev_id, 0, 1, buffer);
    if (ret < 0)
        return ret;

    mbr_t* mbr = buffer;
    mbr_partition_t* part;

    if (mbr->signature != 0xAA55)
        return ERR_NOT_POSSIBLE;

    char name_buffer[16];

    for (int i = 0; i < 4; i++) {
        part = &(mbr->partitions[i]);

        if (part->type == 0)
            continue;

        int id = find_free_entry();

        dev_part_t* dev_part      = kmalloc(sizeof(dev_part_t));
        dev_disk_t* dev_part_disk = kmalloc(sizeof(dev_disk_t));

        dev_part->self_dev_disk = dev_part_disk;
        dev_part->proxy_to      = dev_disk_get_data(dev_id);
        dev_part->disk_id = dev_id;
        dev_part->name    = kmalloc(16);

        part_devs[id] = dev_part;

        dev_id2name(dev_id, name_buffer);
        sprintf(dev_part->name, "%s%i", name_buffer, i + 1);
        
        printf("Partition %i (/dev/%s)\n", i, dev_part->name);
        printf("  Type     : 0x%x\n", part->type);
        printf("  LBA Start: %i\n", part->lba_start);
        printf("  LBA Count: %i\n", part->lba_count);

        dev_part_disk->proxy_to     = dev_part->proxy_to;
        dev_part_disk->proxy_offset = part->lba_start;

        int reg_result = dev_register_disk(dev_part->name, dev_part_disk);

        if (reg_result < 0) {
            printf("/dev/%s: Registration failed\n", dev_part->name);
            continue;
        }
        dev_part->self_dev_id = reg_result;
    }
    kfree(buffer);
    return 0;
}