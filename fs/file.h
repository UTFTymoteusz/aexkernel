#pragma once

#include "fs/inode.h"
#include "fs/pipe.h"

enum {
    FILE_TYPE_NORMAL = 0,
    FILE_TYPE_PIPE   = 1,
};

struct file {
    uint64_t position;
    uint8_t  type;
    
    uint8_t flags;

    union {
        inode_t* inode;
        pipe_t*  pipe;
    };
};
typedef struct file file_t;