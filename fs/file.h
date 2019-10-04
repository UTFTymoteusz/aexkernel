#pragma once

#include "fs/inode.h"

struct file {
    uint64_t position;

    inode_t* inode;
};
typedef struct file file_t;