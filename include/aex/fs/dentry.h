#pragma once

struct dentry {
    char name[256];

    uint8_t type;
    uint64_t inode_id;

    uint64_t blocks;
    uint64_t size;
};
typedef struct dentry dentry_t;