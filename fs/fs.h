#pragma once

#include "aex/klist.h"

#include "fs/dentry.h"
#include "fs/file.h"
#include "fs/inode.h"

#include "stdbool.h"
#include "stddef.h"
#include "stdint.h"

enum fs_flag {
    FS_NODEV    = 0x0001,
    FS_READONLY = 0x0002,
};
enum fs_record_type {
    FS_RECORD_TYPE_FILE  = 2,
    FS_RECORD_TYPE_DIR   = 3,
    FS_RECORD_TYPE_DEV   = 4,
    FS_RECORD_TYPE_MOUNT = 5,
};

struct filesystem_mount {
    uint64_t id;

    uint64_t parent_inode_id;
    uint64_t parent_mount_id;

    char* mount_path;
    char volume_label[72];

    int dev_id;
    uint16_t flags;

    uint64_t block_size;
    uint64_t block_amount;

    uint64_t max_filesize;

    void* private_data;

    struct klist inode_cache;

    int (*readb)(inode_t* inode, uint64_t lblock, uint16_t count, uint8_t* buffer);
    int (*writeb)(inode_t* inode, uint64_t lblock, uint16_t count, uint8_t* buffer);

    uint64_t (*getb)(inode_t* inode, uint64_t lblock);
    //uint64_t (*setb)(inode_t* inode, uint64_t lblock, uint64_t to);
    //uint64_t (*findfb)(inode_t* inode);

    int (*get_inode)(uint64_t id, inode_t* parent, inode_t* inode_target);

    int (*countd)(inode_t* inode);
    int (*listd)(inode_t* inode, dentry_t* dentries, int max);
};
struct filesystem {
    char name[24];
    int (*mount)(struct filesystem_mount*);
};

struct klist filesystems;

void fs_init();

void fs_register(struct filesystem* fssys);

int fs_mount(char* dev, char* path, char* type);
int fs_get_mount(char* path, char* new_path, struct filesystem_mount** mount);

int  fs_get_inode(char* path, inode_t** inode);
void fs_retire_inode(inode_t* inode);

int fs_count(char* path);
int fs_list(char* path, dentry_t* dentries, int max);

bool fs_exists(char* path);
int  fs_open(char* path, file_t* file);
int  fs_read(file_t* file, uint8_t* buffer, int len);
int  fs_write(file_t* file, uint8_t* buffer, int len);
int  fs_seek(file_t* file, uint64_t pos);
void fs_close(file_t* file);
long fs_ioctl(file_t* file, long code, void* mem);