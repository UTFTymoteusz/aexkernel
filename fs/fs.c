#pragma once

#include "aex/klist.h"

#include "dev/name.h"
#include "fs/inode.h"

enum fs_flag {
    FS_NODEV = 1,
};

struct filesystem_mounted {
    char volume_label[72];

    int dev_id;

    uint64_t block_size;
    uint64_t block_amount;

    uint64_t max_filesize;

    void* private_data;

    int (*readb)(inode_t* inode, uint64_t block, uint8_t* buffer);
    int (*writeb)(inode_t* inode, uint64_t block, uint8_t* buffer);

    //int (*listd)(inode_t* inode, uint8_t** data); Needs more consideration
};

struct filesystem {
    char name[24];
    int (*mount)(struct filesystem_mounted*);
};

struct klist filesystems;

int fs_index;

void fs_init() {

    fs_index = 0;
    klist_init(&filesystems);
}

void fs_register(struct filesystem* fssys) {

    klist_set(&filesystems, fs_index++, fssys);
    printf("Registered filesystem '%s'\n", fssys->name);
}

int fs_mount(char* dev, char* path, char* type) {

    klist_entry_t* klist_entry = NULL;
    struct filesystem* fssys   = NULL;

    if (path == NULL)
        return -1;

    int dev_id = dev_name2id(dev);
    if (dev_id < 0)
        return -1;

    struct filesystem_mounted* new_mount = kmalloc(sizeof(struct filesystem_mounted));
    memset(new_mount, 0, sizeof(struct filesystem_mounted));

    while (true) {
        fssys = klist_iter(&filesystems, &klist_entry);

        if (fssys == NULL)
            break;

        if (type == NULL || (!strcmp(fssys->name, type))) {

            new_mount->dev_id = dev_id;

            int ret = fssys->mount(new_mount);

            if (ret >= 0) {
                printf("fs '%s' worked on /dev/%s\n", fssys->name, dev);
                return 0;
            }
        }
    }
    kfree(new_mount);

    return -1;
}