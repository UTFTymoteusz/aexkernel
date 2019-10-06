#pragma once

#include "aex/rcode.h"

#include "fs/fs.h"
#include "fs/inode.h"

int devfs_mount_nodev(struct filesystem_mount* mount);

struct filesystem devfs_filesystem = {
    .name = "devfs",
    .mount = devfs_mount_nodev,
};

void devfs_init() {
    fs_register(&devfs_filesystem);
}

int devfs_countd(inode_t* inode) {
    switch (inode->id) {
        case 1:
            return dev_current_amount();
        default:
            break;
    }
    return 0;
}

int devfs_listd_root(dentry_t* dentries, int max) {
    int dev_amnt = dev_current_amount();
    if (dev_amnt < max)
        max = dev_amnt;
        
    dev_t** devs = kmalloc(dev_amnt * sizeof(dev_t*));
    dev_list(devs);
    
    for (int i = 0; i < max; i++) {
        strcpy(dentries[i].name, devs[i]->name);

        dentries[i].inode_id = 100000 + i;
        dentries[i].type     = FS_RECORD_TYPE_DEV;
    }
    kfree(devs);
    return max;
}

int devfs_listd(inode_t* inode, dentry_t* dentries, int max) {
    switch (inode->id) {
        case 1:
            return devfs_listd_root(dentries, max);
        default:
            break;
    }
    return 0;
}

int devfs_get_inode(uint64_t id, inode_t* parent, inode_t* inode_target) {
    parent = parent;

    if (id == 1) {
        inode_target->parent_id = 0;
        inode_target->type = FS_RECORD_TYPE_DIR;

        return 0;
    }
    else if (id >= 100000) {
        inode_target->parent_id = 1;
        inode_target->type   = FS_RECORD_TYPE_DEV;
        inode_target->dev_id = id - 100000;

        return 0;
    }
    return 0;
}

int devfs_mount_nodev(struct filesystem_mount* mount) {
    mount->get_inode = devfs_get_inode;
    mount->countd    = devfs_countd;
    mount->listd     = devfs_listd;

    if (mount->dev_id >= 0)
        return ERR_NOT_POSSIBLE;

    return 0;
}