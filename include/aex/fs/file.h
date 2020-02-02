#pragma once

struct file;
typedef struct file file_t;

struct finfo;
typedef struct finfo finfo_t;

#include <stdbool.h>

#include "aex/vals/ioctl_names.h"

#include "aex/fs/inode.h"
#include "aex/fs/pipe.h"

enum {
    FILE_TYPE_NORMAL = 0,
    FILE_TYPE_PIPE   = 1,
};
enum {
    FILE_FLAG_READ   = 0x0001,
    FILE_FLAG_WRITE  = 0x0002,
    FILE_FLAG_CLOSED = 0x0008,
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

inline void file_add_reference(file_t* file);
inline bool file_sub_reference(file_t* file);

int  fs_info(char* path, finfo_t* finfo);
bool fs_exists(char* path);

int  fs_open (char* path, file_t* file);
int  fs_read (file_t* file, uint8_t* buffer, int len);
int  fs_write(file_t* file, uint8_t* buffer, int len);
void fs_close(file_t* file);

int  fs_seek (file_t* file, uint64_t pos);
long fs_ioctl(file_t* file, long code, void* mem);


inline void file_add_reference(file_t* file) {
    __sync_add_and_fetch(&(file->ref_count), 1);
}

inline bool file_sub_reference(file_t* file) {
    int res = __sync_sub_and_fetch(&(file->ref_count), 1);
    return res == 0;
}