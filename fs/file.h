#pragma once

struct file;
typedef struct file file_t;

struct finfo;
typedef struct finfo finfo_t;

#include <stdbool.h>

#include "fs/inode.h"
#include "fs/pipe.h"

enum {
    FILE_TYPE_NORMAL = 0,
    FILE_TYPE_PIPE   = 1,
};
enum {
    FILE_FLAG_READ  = 0x0001,
    FILE_FLAG_WRITE = 0x0002,
};

struct file {
    uint64_t position;
    uint8_t  type;
    
    uint16_t flags;
    uint32_t ref_count;

    void* private_data;

    union {
        inode_t* inode;
        pipe_t*  pipe;
    };
};
typedef struct file file_t;

struct finfo {
    uint64_t inode;
    uint8_t  type;
};
typedef struct finfo finfo_t;

bool fs_exists(char* path);
int  fs_info(char* path, finfo_t* finfo);

int  fs_open(char* path, file_t* file);
int  fs_read(file_t* file, uint8_t* buffer, int len);
int  fs_write(file_t* file, uint8_t* buffer, int len);
void fs_close(file_t* file);

int  fs_seek(file_t* file, uint64_t pos);
long fs_ioctl(file_t* file, long code, void* mem);