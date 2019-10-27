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

    switch (inode->type) {
        case FS_RECORD_TYPE_DEV:
            ret = dev_open(inode->dev_id);
            if (ret < 0)
                return ret;

            break;
    }
    //printf("File size: %i\n", inode->size);
    return 0;
}

inline int64_t fs_clamp(int64_t val, int64_t max) {
    if (val > max)
        return max;
    if (val < 0)
        return 0;

    return val;
}

// Replace read and write switches with function pointers
int fs_read_internal(inode_t* inode, uint64_t sblock, int64_t len, uint64_t soffset, uint8_t* buffer) {
    struct filesystem_mount* mount = inode->mount;
    uint32_t block_size = mount->block_size;
    uint32_t max_c      = (65536 / block_size) - 1;

    int ret;

    if (soffset != 0) {
        void* tbuffer = kmalloc(block_size);

        ret = mount->readb(inode, sblock, 1, tbuffer);
        if (ret < 0)
            return ret;

        sblock++;
        memcpy(buffer, tbuffer + soffset, fs_clamp(len, block_size - soffset));

        buffer += (block_size - soffset);
        len    -= (block_size - soffset);
        
        kfree(tbuffer);
    }
    if (len <= 0)
        return 0;

    uint64_t curr_b, last_b, cblock;
    uint16_t amnt  = 0;
    int64_t  lenp = len;

    last_b = mount->getb(inode, sblock) - 1;
    cblock = sblock;

    while (true) {
        curr_b = mount->getb(inode, sblock++);
        len   -= block_size;
        amnt++;

        if (((last_b + 1) != curr_b) || (len <= 0) || (amnt >= max_c)) {
            if (lenp < block_size) {
                void* tbuffer = kmalloc(block_size);

                ret = mount->readb(inode, cblock, amnt, tbuffer);
                if (ret < 0)
                    return ret;

                memcpy(buffer, tbuffer, lenp);
                kfree(tbuffer);
            }
            else
                ret = mount->readb(inode, cblock, amnt, buffer);

            if (ret < 0)
                return ret;

            if (len <= 0)
                return 0;

            buffer += amnt * block_size;

            amnt   = 0;
            cblock = sblock;
        }
        lenp -= block_size;
        last_b = curr_b;
    }
    if (len <= 0)
        return 0;

    void* tbuffer = kmalloc(block_size);

    ret = mount->readb(inode, sblock, 1, tbuffer);
    if (ret < 0)
        return ret;

    memcpy(buffer, tbuffer, fs_clamp(len, block_size));
    kfree(tbuffer);

    return 0;
}

int fs_read(file_t* file, uint8_t* buffer, int len) {
    if (len == 0)
        return 0;

    inode_t* inode = file->inode;

    uint64_t size = inode->size;
    uint64_t lent = len;

    if (file->position + lent >= size)
        lent = size - file->position;

    if (file->position == size)
        return 0;

    uint32_t block_size = inode->mount->block_size;
    uint64_t dst = file->position + lent;
    uint64_t starting_block = (file->position + 1) / block_size;
    uint32_t start_offset   = file->position - starting_block * block_size;

    fs_read_internal(inode, starting_block, lent, start_offset, buffer);

    file->position = dst;
    return lent;
}

int fs_write(file_t* file, uint8_t* buffer, int len) {
    if (len == 0)
        return 0;

    inode_t* inode = file->inode;

    uint64_t lent = len;

    switch (inode->type) {
        case FS_RECORD_TYPE_FILE:
            kpanic("File writing is not yet implemented");
            break;
        case FS_RECORD_TYPE_DEV:
            return dev_write(inode->dev_id, buffer, len);
            break;
        default:
            kpanic("Invalid record type");
            break;
    }
    return lent;
}

int fs_seek(file_t* file, uint64_t pos) {
    file->position = pos;
    return 0;
}

void fs_close(file_t* file) {
    inode_t* inode = file->inode;

    switch (inode->type) {
        case FS_RECORD_TYPE_DEV:
            dev_close(inode->dev_id);
            break;
    }
    fs_retire_inode(inode);
}

long fs_ioctl(file_t* file, long code, void* mem) {
    inode_t* inode = file->inode;

    switch (inode->type) {
        case FS_RECORD_TYPE_DEV:
            return dev_ioctl(inode->dev_id, code, mem);
        default:
            return FS_ERR_NOT_A_DEV;
    }
}