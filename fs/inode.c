#pragma once

#include "fs/ilocation.h"

struct inode {
    uint64_t id;

    uint64_t size;
    uint64_t blocks;

    uint64_t first_block;

    uint8_t type;

    struct filesystem_mount* mount;
    uint64_t parent_id;

    int32_t references;

    struct ilocation* location;
} __attribute((packed));
typedef struct inode inode_t;