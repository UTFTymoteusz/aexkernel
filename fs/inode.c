#pragma once

#include "fs/ilocation.c"

struct inode {
    uint64_t id;

    uint64_t size;
    uint64_t blocks;

    uint8_t type;

    struct inode* parent;

    void* data;
    struct ilocation* location;
} __attribute((packed));
typedef struct inode inode_t;

inline inode_t* inode_alloc() {
    return kmalloc(sizeof(inode_t));
}