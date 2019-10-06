#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum disk_flags {
    DISK_PARTITIONABLE = 0x0001,
    DISK_BOOTED_FROM   = 0x0002,
};

struct dev_disk {
    int internal_id;
    bool initialized;

    char model_name[64];

    uint16_t flags;
    uint16_t max_sectors_at_once;

    uint32_t total_sectors;
    uint32_t sector_size;

    struct dev_disk_ops* disk_ops;

    struct dev_disk* proxy_to;
    uint64_t proxy_offset;
};
typedef struct dev_disk dev_disk_t;

struct dev_disk_ops {
    int (*init) (int drive);
    int (*read) (int drive, uint64_t sector, uint16_t count, uint8_t* buffer);
    int (*write)(int drive, uint64_t sector, uint16_t count, uint8_t* buffer);
    void (*release)(int drive);

    long (*ioctl)(int drive, long, long);
};

int  dev_register_disk(char* name, struct dev_disk* disk);
void dev_unregister_disk(char* name);

int  dev_disk_init(int dev_id);
int  dev_disk_read(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer);
int  dev_disk_dread(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer);
int  dev_disk_write(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer);
void dev_disk_release(int dev_id);

dev_disk_t* dev_disk_get_data(int dev_id);