#include "aex/klist.h"
#include "aex/kmem.h"
#include "aex/rcode.h"

#include "dev/cpu.h"
#include "dev/dev.h"
#include "dev/name.h"

#include "fs/dentry.h"
#include "fs/file.h"
#include "fs/inode.h"

#include "kernel/sys.h"
#include "kernel/syscall.h"

#include "proc/proc.h"

#include <stdio.h>
#include <string.h>

#include "fs.h"

enum fs_flag;
enum fs_record_type;

struct filesystem;
struct filesystem_mount;

struct klist filesystems;
struct klist mounts;

int fs_index, mnt_index;

long syscall_fopen(char* path, int mode) {
    nointerrupts();

    int fd = process_current->fiddie_counter++;
    file_t* file = kmalloc(sizeof(file_t));

    int ret = fs_open(path, file);
    if (ret < 0) {
        kfree(file);
        return ret;
    }
    klist_set(&(process_current->fiddies), fd, file);

    interrupts();
    return fd;
}

long syscall_fread(long fd, uint8_t* buffer, int len) {
    file_t* file = klist_get(&(process_current->fiddies), fd);
    if (file == NULL)
        return FS_ERR_NOT_OPEN;

    return fs_read(file, buffer, len);
}

long syscall_fwrite(long fd, uint8_t* buffer, int len) {
    file_t* file = klist_get(&(process_current->fiddies), fd);
    if (file == NULL)
        return FS_ERR_NOT_OPEN;

    return fs_write(file, buffer, len);
}

long syscall_fclose(long fd) {
    file_t* file = klist_get(&(process_current->fiddies), fd);
    if (file == NULL)
        return FS_ERR_NOT_OPEN;

    klist_set(&(process_current->fiddies), fd, NULL);
    kfree(file);
    return 0;
}

long syscall_fseek(long fd, uint64_t pos) {
    file_t* file = klist_get(&(process_current->fiddies), fd);
    if (file == NULL)
        return FS_ERR_NOT_OPEN;

    return fs_seek(file, pos);
}

void fs_init() {
    fs_index  = 0;
    mnt_index = 0;

    klist_init(&filesystems);
    klist_init(&mounts);

    syscalls[SYSCALL_FOPEN]  = syscall_fopen;
    syscalls[SYSCALL_FREAD]  = syscall_fread;
    syscalls[SYSCALL_FWRITE] = syscall_fwrite;
    syscalls[SYSCALL_FCLOSE] = syscall_fclose;
    syscalls[SYSCALL_FSEEK]  = syscall_fseek;
}

void fs_register(struct filesystem* fssys) {
    klist_set(&filesystems, fs_index++, fssys);
    printf("Registered filesystem '%s'\n", fssys->name);
}

int fs_get_mount(char* path, char* new_path, struct filesystem_mount** mount);

static int fs_get_inode_internal(char* path, inode_t* inode) {
    char* path_new = kmalloc(strlen(path) + 1);
    struct filesystem_mount* mount;
    fs_get_mount(path, path_new, &mount);

    inode_t* inode_p = kmalloc(sizeof(inode_t));

    memset(inode_p, 0, sizeof(inode_t));
    memset(inode, 0, sizeof(inode_t));

    inode_p->id        = 0;
    inode_p->parent_id = 0;
    inode_p->mount     = mount;

    inode->id    = 1;
    inode->mount = mount;

    mount->get_inode(1, inode_p, inode);

    int guard   = strlen(path_new);
    char* piece = kmalloc(256);

    if (path_new[guard] == '/')
        guard--;

    int i = 0;
    int j = 0;

    int current_level = 0;
    int destination_level = 0;

    char c;
    for (int k = 0; k < guard; k++)
        if (path_new[k] == '/')
            ++destination_level;

    while (i <= guard) {
        c = path_new[i];

        if (c == '/' || i == guard) {
            int count  = inode->mount->countd(inode);
            bool found = false;

            ++i;
            piece[j] = '\0';

            if (strlen(piece) == 0)
                break;

            dentry_t* dentries = kmalloc(sizeof(dentry_t) * count);
            inode->mount->listd(inode, dentries, count);

            for (int k = 0; k < count; k++) {
                if (!strcmp(piece, dentries[k].name)) {
                    if (dentries[k].type != FS_RECORD_TYPE_DIR && current_level != destination_level)
                        break;

                    uint64_t next_inode_id = dentries[k].inode_id;

                    memcpy(inode_p, inode, sizeof(inode_t));

                    inode->id    = next_inode_id;
                    inode->mount = mount;
                    inode->type  = dentries[k].type;

                    found = true;

                    int ret = mount->get_inode(next_inode_id, inode_p, inode);
                    if (ret < 0) {
                        kfree(dentries);
                        kfree(piece);
                        kfree(inode_p);
                        kfree(path_new);

                        return ret;
                    }
                    found = true;
                }
            }
            j = 0;

            kfree(dentries);

            if (!found)
                return FS_ERR_NOT_FOUND;

            if (current_level == destination_level)
                break;

            ++current_level;
            continue;
        }
        piece[j++] = c;
        ++i;
    }
    kfree(piece);
    kfree(inode_p);
    kfree(path_new);

    return current_level;
}

int fs_get_inode(char* path, inode_t** inode) {
    *inode = kmalloc(sizeof(inode_t));

    int ret = fs_get_inode_internal(path, *inode);
    if (ret < 0) {
        kfree(*inode);
        return ret;
    }
    klist_set(&((*inode)->mount->inode_cache), (*inode)->id, *inode);

    //printf("inode '%i' got cached\n", (*inode)->id);

    (*inode)->references++;
    return ret;
}

void fs_retire_inode(inode_t* inode) {
    inode->references--;

    if (inode->references <= 0) {
        inode_t* cached = klist_get(&(inode->mount->inode_cache), inode->id);

        if (cached != NULL) {
            klist_set(&(inode->mount->inode_cache), inode->id, NULL);
            //printf("inode '%i' got dropped\n", inode->id);
        }
        kfree(inode);
    }
}

int fs_mount(char* dev, char* path, char* type) {
    klist_entry_t* klist_entry = NULL;
    struct filesystem* fssys   = NULL;

    if (path == NULL)
        return ERR_INV_ARGUMENTS;

    int dev_id = -1;

    if (dev != NULL) {
        dev_id = dev_name2id(dev);
        if (dev_id < 0)
            return DEV_ERR_NOT_FOUND;
    }
    struct filesystem_mount* new_mount = kmalloc(sizeof(struct filesystem_mount));
    memset(new_mount, 0, sizeof(struct filesystem_mount));

    if (mounts.count > 0) {
        int wlen = strlen(path);
        char* work_path = kmalloc(wlen + 1);

        strcpy(work_path, path);

        if (work_path[wlen - 1] == '/')
            work_path[--wlen] = '\0';

        int wi = wlen;

        while (work_path[wi] != '/')
            work_path[wi--] = '\0';

        inode_t* inode_mntp;

        int ret = fs_get_inode(work_path, &inode_mntp);
        if (ret < 0) {
            kfree(new_mount);
            kfree(work_path);

            return ret;
        }
        new_mount->parent_inode_id = inode_mntp->id;
        new_mount->parent_mount_id = inode_mntp->mount->id;

        fs_retire_inode(inode_mntp);
        kfree(work_path);
    }
    while (true) {
        fssys = klist_iter(&filesystems, &klist_entry);

        if (fssys == NULL)
            break;

        if (type == NULL || (!strcmp(fssys->name, type))) {
            new_mount->dev_id = dev_id;

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

                if (dev_id >= 0)
                    printf("fs '%s' worked on /dev/%s, mounted it on %s\n", fssys->name, dev, new_mount->mount_path);
                else
                    printf("fs '%s' worked without a device, mounted it on %s\n", fssys->name, new_mount->mount_path);

                new_mount->id = mnt_index;

                klist_set(&mounts, mnt_index++, new_mount);
                return 0;
            }
        }
    }
    kfree(new_mount);
    return FS_ERR_NO_MATCHING_FILESYSTEM;
}

int fs_get_mount(char* path, char* new_path, struct filesystem_mount** mount) {
    if (path == NULL)
        return ERR_INV_ARGUMENTS;

    klist_entry_t* klist_entry = NULL;
    struct filesystem_mount* fsmnt;

    int longest = -1;
    int bigbong = -1;
    int len     = strlen(path);
    int len2    = len;

    if (path[len - 1] == '/')
        --len;

    int mnt_len;

    while (true) {
        fsmnt = klist_iter(&mounts, &klist_entry);
        if (fsmnt == NULL)
            break;

        mnt_len = strlen(fsmnt->mount_path) - 1;

        if (!memcmp(path, fsmnt->mount_path, mnt_len) && mnt_len > longest) {
            longest = mnt_len + 1;
            bigbong = klist_entry->index;
        }
    }
    if (bigbong == -1)
        return FS_ERR_NOT_FOUND;

    memcpy(new_path, path + longest, (len2 + 1) - longest);

    *mount = klist_get(&mounts, bigbong);
    return 0;
}

int fs_count(char* path) {
    inode_t* inode = NULL;

    int ret = fs_get_inode(path, &inode);
    if (ret < 0)
        return ret;

    if (inode->type != FS_RECORD_TYPE_DIR && inode->type != FS_RECORD_TYPE_MOUNT)
        return FS_ERR_NOT_DIR;

    ret = inode->mount->countd(inode);
    if (ret < 0) {
        fs_retire_inode(inode);
        return ret;
    }
    klist_entry_t* klist_entry = NULL;
    struct filesystem_mount* fsmnt;

    while (true) {
        fsmnt = klist_iter(&mounts, &klist_entry);
        if (fsmnt == NULL)
            break;

        if (fsmnt->parent_inode_id == inode->id
         && fsmnt->parent_mount_id == inode->mount->id)
            ++ret;
    }
    fs_retire_inode(inode);
    return ret;
}

int fs_list(char* path, dentry_t* dentries, int max) {
    inode_t* inode = NULL;

    int ret = fs_get_inode(path, &inode);
    if (ret < 0)
        return ret;

    if (inode->type != FS_RECORD_TYPE_DIR && inode->type != FS_RECORD_TYPE_MOUNT)
        return FS_ERR_NOT_DIR;
        
    ret = inode->mount->listd(inode, dentries, max);
    if (ret < 0) {
        fs_retire_inode(inode);
        return ret;
    }
    klist_entry_t* klist_entry = NULL;
    struct filesystem_mount* fsmnt;

    while (true) {
        if (ret >= max)
            break;

        fsmnt = klist_iter(&mounts, &klist_entry);
        if (fsmnt == NULL)
            break;

        if (fsmnt->parent_inode_id == inode->id
         && fsmnt->parent_mount_id == inode->mount->id)
        {
            char* name = fsmnt->mount_path + strlen(fsmnt->mount_path) - 2;

            while (*name != '/')
                --name;

            name++;

            char* dname = dentries[ret].name;
            strcpy(dname, name);
            dname[strlen(dname) - 1] = '\0';

            dentries[ret].type = FS_RECORD_TYPE_MOUNT;
            dentries[ret].inode_id = 1;

            ++ret;
        }
    }
    fs_retire_inode(inode);
    return ret;
}

bool fs_fexists(char* path) {
    inode_t* inode = NULL;

    int ret = fs_get_inode(path, &inode);
    if (ret < 0) {
        fs_retire_inode(inode);
        return false;
    }
    fs_retire_inode(inode);
    return true;
}