#include "aex/kmem.h"
#include "aex/rcode.h"

#include "dev/block.h"

#include "fs/fs.h"
#include "fs/inode.h"

#include <stdio.h>
#include <stdint.h>

#include "fat.h"

int fat_mount_dev(struct filesystem_mount* mount);

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

struct fat_ebp_32 {
    uint32_t fat_sector_size;
    uint16_t flags;
    uint16_t version;

    uint32_t root_cluster;
    uint16_t fsinfo_sector;
    uint16_t boot_backup_sector;

    uint8_t reserved[12];

    uint8_t drive_number;
    uint8_t flags_windowsnt;
    uint8_t signature;

    uint32_t volume_serial_number;
    char volume_label[11];
    char identifier[8];

    uint8_t code[420];
    uint16_t boot_signature;
} __attribute((packed));

void fat_init() {
    fs_register(&fat_filesystem);
}

int fat_mount_dev(struct filesystem_mount* mount) {
    void* yeet = kmalloc(2048);
    struct fat_bpb* bpb = yeet;

    dev_block_read(mount->dev_id, 0, 4, yeet);

    if (bpb->bytes_per_sector != 512) {
        printf("Implement FAT for sector sizes other than 512 bytes pls\n");
        return ERR_GENERAL;
    }
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
    return ERR_GENERAL;
}