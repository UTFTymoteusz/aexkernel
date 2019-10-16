#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum block_flags {
    DISK_PARTITIONABLE = 0x0001,
    DISK_BOOTED_FROM   = 0x0002,
};

struct dev_block {
    int internal_id;
    bool initialized;

    char model_name[64];

    uint16_t flags;
    uint16_t max_sectors_at_once;

    uint32_t total_sectors;
    uint32_t sector_size;

    struct dev_block_ops* block_ops;

    struct dev_block* proxy_to;
    uint64_t proxy_offset;
};
typedef struct dev_block dev_block_t;

struct dev_block_ops {
    int (*init) (int drive);
    int (*read) (int drive, uint64_t sector, uint16_t count, uint8_t* buffer);
    int (*write)(int drive, uint64_t sector, uint16_t count, uint8_t* buffer);
    void (*release)(int drive);

    long (*ioctl)(int drive, long, long);
};

int  dev_register_block(char* name, struct dev_block* block_dev);
void dev_unregister_block(char* name);

int  dev_block_init(int dev_id);
int  dev_block_read(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer);
int  dev_block_dread(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer);
int  dev_block_write(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer);
void dev_block_release(int dev_id);

dev_block_t* dev_block_get_data(int dev_id);