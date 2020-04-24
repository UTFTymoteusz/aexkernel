#pragma once

#include "aex/dev/tree.h"

#include <stdint.h>

enum block_type {
    DISK_TYPE_INVALID = 0x00,
    DISK_TYPE_DISK    = 0x01,
    DISK_TYPE_OPTICAL = 0x02,
};

struct class_disk {
    int type;

    int ifd;
    char name[16];

    uint64_t sector_count;
    uint16_t sector_size;
    uint16_t burst_max;

    int (*init)   (device_t* dev);
    int (*read)   (device_t* dev, uint64_t sector, uint16_t count, uint8_t* buffer);
    int (*write)  (device_t* dev, uint64_t sector, uint16_t count, uint8_t* buffer);
    int (*release)(device_t* dev);
};
typedef struct class_disk class_disk_t;

int disk_get_type(char* name);