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
    
    // make an union for directory listing and data pointers or idk

    struct ilocation* location;
} __attribute((packed));
typedef struct inode inode_t;

inline inode_t* inode_alloc() {
    return kmalloc(sizeof(inode_t));
}