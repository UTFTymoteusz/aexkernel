#pragma once

#include "fs/fs.h"
#include "fs/inode.h"

int fat_mount_dev(struct filesystem_mounted* mounted);

struct filesystem fat_filesystem = {
    .name = "fat",
    .mount = fat_mount_dev,
};

struct fat_bpb {
    uint8_t jump_code[3];
    char    oem_id[8];

    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;

    uint16_t reserved_sectors;
    uint8_t  fat_copies;
    uint16_t directory_entries;

    uint16_t total_sectors;
    uint8_t  media_type;

    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t head_amount;

    uint32_t hidden_sector_amount;
    uint32_t large_total_sectors;
} __attribute((packed));

void fat_init() {

    fs_register(&fat_filesystem);

}

int fat_mount_dev(struct filesystem_mounted* mounted) {
    
    void* yeet = kmalloc(2048);
    
    dev_disk_read(mounted->dev_id, 0, 4, yeet);

    struct fat_bpb* bpb = yeet;

    if (bpb->bytes_per_sector != 512)
        kpanic("Implement FAT for sector sizes other than 512 bytes pls");

    printf("FAT Mount Data\n");
    printf("  Bytes per sector   : %i\n", bpb->bytes_per_sector);
    printf("  Sectors per cluster: %i\n", bpb->sectors_per_cluster);
    printf("  Reserved sectors   : %i\n", bpb->reserved_sectors);
    printf("  FAT copies         : %i\n", bpb->fat_copies);
    printf("  Directory entries  : %i\n", bpb->directory_entries);
    printf("  Total sectors      : %i\n", bpb->total_sectors);
    printf("  Media type         : %x\n", bpb->media_type);
    printf("  Old FAT sectors    : %i\n", bpb->sectors_per_fat);
    printf("  Sectors per track  : %i\n", bpb->sectors_per_track);
    printf("  Head amount        : %i\n", bpb->head_amount);
    printf("  Hidden sectors     : %i\n", bpb->hidden_sector_amount);
    printf("  32bit total sectors: %i\n", bpb->large_total_sectors);
    printf("\n");

    kfree(yeet);

    return -1;
}