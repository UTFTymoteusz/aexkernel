#pragma once

#include "aex/aex.h"

#include "ilocation.h"

struct inode {
    uint64_t id;

    uint64_t size;
    uint64_t blocks;

    uint8_t type;

    union {
        uint64_t first_block;
        int dev_id;
    };
    struct filesystem_mount* mount;
    uint64_t parent_id;

    int32_t references;

    struct ilocation* location;
};
typedef struct inode inode_t;