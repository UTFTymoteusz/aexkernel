#pragma once

#include "aex/klist.h"

#include "dev/name.h"

#include "fs/file.h"
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

    struct klist inode_cache;

    int (*readb)(inode_t* inode, uint64_t block, uint16_t count, uint8_t* buffer);
    int (*writeb)(inode_t* inode, uint64_t block, uint16_t count, uint8_t* buffer);

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
        return ERR_INV_ARGUMENTS;

    int dev_id = dev_name2id(dev);
    if (dev_id < 0)
        return DEV_ERR_NOT_FOUND;

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
                
                klist_init(&(new_mount->inode_cache));
                
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

    return FS_ERR_NO_MATCHING_FILESYSTEM;
}
int fs_get_mount(char* path, struct filesystem_mount** mount) {

    if (path == NULL)
        return ERR_INV_ARGUMENTS;

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
        return FS_ERR_NOT_FOUND;

    *mount = klist_get(&mounts, bigbong);

    return 0;
}

#include "fs_inode_find.c"

int fs_get_inode(char* path, inode_t** inode) {
    *inode = kmalloc(sizeof(inode_t));

    int ret = fs_get_inode_internal(path, *inode);
    if (ret < 0) {
        kfree(*inode);
        return ret;
    }

    klist_set(&((*inode)->mount->inode_cache), (*inode)->id, *inode);

    printf("inode '%i' got cached\n", (*inode)->id);

    (*inode)->references++;

    return ret;
}
void fs_retire_inode(inode_t* inode) {
    inode->references--;

    if (inode->references <= 0) {
        inode_t* cached = klist_get(&(inode->mount->inode_cache), inode->id);

        if (cached != NULL) {
            klist_set(&(inode->mount->inode_cache), inode->id, NULL);
            printf("inode '%i' got dropped\n", inode->id);
        }

        kfree(inode);
    }
}

int fs_count(char* path) {
    inode_t* inode = NULL;

    int ret = fs_get_inode(path, &inode);
    if (ret < 0)
        return ret;

    ret = inode->mount->countd(inode);
    fs_retire_inode(inode);

    return ret;
}
int fs_list(char* path, dentry_t* dentries, int max) {
    inode_t* inode = NULL;

    int ret = fs_get_inode(path, &inode);
    if (ret < 0)
        return ret;

    ret = inode->mount->listd(inode, dentries, max);
    fs_retire_inode(inode);

    return ret;
}

int fs_open(char* path, file_t* file) {
    inode_t* inode = NULL;

    int ret = fs_get_inode(path, &inode);
    if (ret < 0)
        return ret;

    if (inode->type == FS_RECORD_TYPE_DIR) {
        fs_retire_inode(inode);
        return FS_ERR_IS_DIR;
    }

    memset(file, 0, sizeof(file_t));
    file->inode = inode;

    return 0;
}

int fs_read(file_t* file, int len, uint8_t* buffer) {

}

void fs_close(file_t* file) {
    inode_t* inode = file->inode;

    fs_retire_inode(inode);
}