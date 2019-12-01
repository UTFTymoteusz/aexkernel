#pragma once

#include "aex/klist.h"

#include "fs/dentry.h"
#include "fs/inode.h"

#include "stddef.h"
#include "stdint.h"

enum fs_flag {
    FS_NODEV    = 0x0001,
    FS_READONLY = 0x0002,
    FS_DONT_CACHE_INODES = 0x0004,
};
enum fs_type {
    FS_TYPE_FILE  = 2,
    FS_TYPE_DIR   = 3,
    FS_TYPE_CDEV  = 4,
    FS_TYPE_BDEV  = 5,
};
enum fs_mode {
    FS_MODE_READ    = 0x0001,
    FS_MODE_WRITE   = 0x0002,
    FS_MODE_EXECUTE = 0x0004,
};

struct hook_file_data {
    char* path;

    int mode;
};
typedef struct hook_file_data hook_file_data_t;

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

void translate_path(char* buffer, char* base, char* path);
long check_user_file_access(char* path, int mode);