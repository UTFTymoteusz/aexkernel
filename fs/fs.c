#pragma once

#include "aex/klist.h"

#include "dev/name.h"

#include "fs/inode.h"
#include "fs/dentry.h"

enum fs_flag {
    FS_NODEV = 1,
};

enum fs_record_type {
    FS_RECORD_TYPE_FILE = 2,
    FS_RECORD_TYPE_DIR  = 3,
};

struct filesystem_mount {
    char* mount_path;
    char volume_label[72];

    int dev_id;

    uint64_t block_size;
    uint64_t block_amount;

    uint64_t max_filesize;

    void* private_data;

    int (*readb)(inode_t* inode, uint64_t block, uint16_t len, uint8_t* buffer);
    int (*writeb)(inode_t* inode, uint64_t block, uint16_t len, uint8_t* buffer);

    int (*get_inode)(uint64_t id, inode_t* parent, inode_t* inode_target);
    int (*free_inode)(inode_t* inode);

    int (*countd)(inode_t* inode);
    int (*listd)(inode_t* inode, dentry_t* dentries, int max);
};

struct filesystem {
    char name[24];
    int (*mount)(struct filesystem_mount*);
};

struct klist filesystems;
struct klist mounts;

int fs_index, mnt_index;

void fs_init() {

    fs_index  = 0;
    mnt_index = 0;
    
    klist_init(&filesystems);
    klist_init(&mounts);
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

    struct filesystem_mount* new_mount = kmalloc(sizeof(struct filesystem_mount));
    memset(new_mount, 0, sizeof(struct filesystem_mount));

    while (true) {
        fssys = klist_iter(&filesystems, &klist_entry);

        if (fssys == NULL)
            break;

        if (type == NULL || (!strcmp(fssys->name, type))) {

            new_mount->dev_id = dev_id;

            //printf("trying fs '%s'\n", fssys->name);

            int ret = fssys->mount(new_mount);

            if (ret >= 0) {

                int path_len = strlen(path);
                if (path[path_len - 1] != '/')
                    path_len++;

                new_mount->mount_path = kmalloc(path_len + 1);
                memcpy(new_mount->mount_path, path, strlen(path));

                new_mount->mount_path[path_len - 1] = '/';
                new_mount->mount_path[path_len] = '\0';
                
                //inode_t* a = kmalloc(sizeof(inode_t));
                //inode_t* b = kmalloc(sizeof(inode_t));

                //a->mount = new_mount;

                //new_mount->get_inode(1, a, b);
                
                printf("fs '%s' worked on /dev/%s, mounted it on %s\n", fssys->name, dev, new_mount->mount_path);

                klist_set(&mounts, mnt_index++, new_mount);

                return 0;
            }
        }
    }
    kfree(new_mount);

    return -1;
}
int fs_get_mount(char* path, struct filesystem_mount** mount) {

    if (path == NULL)
        return -1;

    klist_entry_t* klist_entry = NULL;
    struct filesystem_mount* fsmnt;

    int bigbong = -1;
    int len     = strlen(path);

    if (path[len - 1] == '/')
        --len;

    uint16_t longest = 0;

    int mnt_len;

    while (true) {
        fsmnt = klist_iter(&mounts, &klist_entry);

        if (fsmnt == NULL)
            break;

        mnt_len = strlen(fsmnt->mount_path) - 1;
        if (mnt_len <= 0)
            mnt_len++;

        //printf("iter '%s' - %i\n", fsmnt->mount_path, mnt_len);

        if (!memcmp(path, fsmnt->mount_path, mnt_len) && mnt_len > longest) {
            longest = mnt_len;
            bigbong = klist_entry->index;
        }
    }
    if (bigbong == -1)
        return -1;

    *mount = klist_get(&mounts, bigbong);

    return 0;
}

int fs_get_inode(char* path, inode_t* inode) {

    struct filesystem_mount* mount;
    fs_get_mount(path, &mount);

    printf("boi is '%s'\n", mount->mount_path);

    inode_t* inode_p = kmalloc(sizeof(inode_t));
    
    memset(inode_p, 0, sizeof(inode_t));
    memset(inode, 0, sizeof(inode_t));

    inode_p->id        = 0;
    inode_p->parent_id = 0;
    inode_p->mount     = mount;

    inode->id    = 1;
    inode->mount = mount;

    mount->get_inode(1, inode_p, inode);

    int jumps = 0;
    int i = 1;
    int j = 0;

    int guard = strlen(path);
    char* piece = kmalloc(256);

    char c;

    while (i <= guard) {
        c = path[i];

        if (c == '/' || i == guard) {
            piece[j] = '\0';

            ++i;

            //printf("N: %s\n", piece);

            int count = inode->mount->countd(inode);
            //printf("C: %i\n", count);

            dentry_t* dentries = kmalloc(sizeof(dentry_t) * count);
            inode->mount->listd(inode, dentries, count);

            for (int k = 0; k < count; k++) {

                if (!strcmp(dentries[k].name, piece)) {

                    uint64_t next_inode_id = dentries[k].inode_id;

                    //printf("NXTINOD: %i\n", next_inode_id);

                    memcpy(inode_p, inode, sizeof(inode_t));

                    inode->id    = next_inode_id;
                    inode->mount = mount;
                    
                    mount->get_inode(next_inode_id, inode_p, inode);
                }
            }

            j = 0;
            continue;
        }

        piece[j++] = c;
        ++i;
    }

    kfree(piece);
    kfree(inode_p);

    return jumps;
}

int fs_count(char* path) {
    inode_t* inode = kmalloc(sizeof(inode_t));

    int ret = fs_get_inode(path, inode);

    printf("inode is %i\n", inode->id);

    if (ret < 0) {
        kfree(inode);
        return ret;
    }

    ret = inode->mount->countd(inode);
    kfree(inode);

    return ret;
}

int fs_list(char* path, dentry_t* dentries, int max) {
    inode_t* inode   = kmalloc(sizeof(inode_t));

    int ret = fs_get_inode(path, inode);

    printf("inode is %i\n", inode->id);

    if (ret < 0) {
        kfree(inode);
        return ret;
    }

    return inode->mount->listd(inode, dentries, max);
}