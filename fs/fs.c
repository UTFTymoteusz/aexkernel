#include "aex/hook.h"
#include "aex/klist.h"
#include "aex/mem.h"
#include "aex/rcode.h"
#include "aex/sys.h"
#include "aex/syscall.h"

#include "aex/dev/dev.h"
#include "aex/dev/name.h"

#include "aex/fs/dentry.h"
#include "aex/fs/file.h"
#include "aex/fs/inode.h"

#include "aex/proc/proc.h"

#include <stdio.h>
#include <string.h>

#include "kernel/init.h"
#include "aex/fs/fs.h"

enum fs_flag;
enum fs_type;

struct filesystem;
struct filesystem_mount;

struct klist filesystems;
struct klist mounts;

int fs_index, mnt_index;

void translate_path(char* buffer, char* base, char* path) {
    if (path[0] == '/' || base == NULL)
        strcpy(buffer, path);
    else {
        if (base[0] != '/')
            sprintf(buffer, "/%s/%s", base, path);
        else
            sprintf(buffer, "%s/%s", base, path);
    }
    char* out  = buffer;

    char specbuff[4];
    uint8_t spb = 0;

    memset(specbuff, '\0', 4);
    
    char* start = out;

    *out = *buffer;
    if (*buffer == '\0')
        return;

    if (*buffer == '/') {
        ++buffer;
        ++out;
    }
    while (true) {
        if (spb < sizeof(specbuff) - 1 && *buffer != '/')
            specbuff[spb++] = *buffer;

        if (*buffer == '/' || *buffer == '\0') {
            if (strcmp(specbuff, "") == 0 && *buffer != '\0')
                out--;
            else if (strcmp(specbuff, "..") == 0) {
                out--;
                while (*out != '/' && out != start)
                    --out;

                if (out != start)
                    --out;

                while (*out != '/' && out != start)
                    --out;

                *out = '/';

                if (*buffer == '\0')
                    ++out;
            }
            else if (strcmp(specbuff, ".") == 0)
                out -= 2;

            memset(specbuff, '\0', 4);
            spb = 0;
        }
        *out = *buffer;

        if (*buffer == '\0')
            break;

        ++buffer;
        ++out;
    }
}

long check_user_file_access(char* path, int mode) {
    void* ptr = NULL;
    hook_first(HOOK_USR_FACCESS, &ptr);

    hook_file_data_t data = {
        .path = path,
        .mode = mode,
    };

    long hret = 0;
    while (ptr != NULL) {
        hret = (long) hook_invoke_advance(HOOK_USR_FACCESS, &data, &ptr);
        if (hret != 0)
            return hret;
    }
    return 0;
}

long syscall_fopen(char* path, int mode) {
    mode &= 0b0111;

    long fret;
    if ((fret = check_user_file_access(path, mode)) != 0)
        return fret;
        
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

bool syscall_fexists(char* path) {
    return fs_exists(path);
}

int syscall_finfo(char* path, finfo_t* finfo) {
    return fs_info(path, finfo);
}

long syscall_ioctl(long fd, long code, void* mem) {
    file_t* file = klist_get(&(process_current->fiddies), fd);
    if (file == NULL)
        return FS_ERR_NOT_OPEN;

    return fs_ioctl(file, code, mem);
}

struct dentry_usr {
    char name[256];
    uint8_t type;
};
typedef struct dentry_usr dentry_usr_t;

int syscall_fcount(char* path) {
    long fret;
    if ((fret = check_user_file_access(path, FILE_FLAG_READ)) != 0)
        return fret;

    return fs_count(path);
}

int syscall_flist(char* path, dentry_usr_t* dentries, int count) {
    long fret;
    if ((fret = check_user_file_access(path, FILE_FLAG_READ)) != 0)
        return fret;
        
    dentry_t* buff = kmalloc(sizeof(dentry_t) * count);

    int ret = fs_list(path, buff, count);
    if (ret < 0) {
        kfree(buff);
        return ret;
    }

    for (int i = 0; i < count; i++) {
        strcpy(dentries[i].name, buff[i].name);
        dentries[i].type = buff[i].type;
    }
    kfree(buff);
    return 0;
}

void fs_init() {
    fs_index  = 0;
    mnt_index = 0;

    klist_init(&filesystems);
    klist_init(&mounts);

    syscalls[SYSCALL_FOPEN]   = syscall_fopen;
    syscalls[SYSCALL_FREAD]   = syscall_fread;
    syscalls[SYSCALL_FWRITE]  = syscall_fwrite;
    syscalls[SYSCALL_FCLOSE]  = syscall_fclose;
    syscalls[SYSCALL_FSEEK]   = syscall_fseek;
    syscalls[SYSCALL_FEXISTS] = syscall_fexists;
    syscalls[SYSCALL_FINFO]   = syscall_finfo;
    syscalls[SYSCALL_FCOUNT]  = syscall_fcount;
    syscalls[SYSCALL_FLIST]   = syscall_flist;
    syscalls[SYSCALL_IOCTL]   = syscall_ioctl;
}

void fs_register(struct filesystem* fssys) {
    klist_set(&filesystems, fs_index++, fssys);
}

int fs_get_mount(char* path, char* new_path, struct filesystem_mount** mount);

static int fs_get_inode_internal(char* path, inode_t* inode) {
    char* path_new = kmalloc(strlen(path) + 1);
    struct filesystem_mount* mount;

    translate_path(path, NULL, path);

    int ret = fs_get_mount(path, path_new, &mount);
    if (ret < 0)
        return ret;

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

            // This here ensures that a / at the end of a path doesn't result in a not-found error
            if (strlen(piece) == 0) {
                if (current_level == destination_level)
                    break;

                j = 0;
                ++current_level;
                continue;
            }
            dentry_t* dentries = kmalloc(sizeof(dentry_t) * count);
            inode->mount->listd(inode, dentries, count);

            for (int k = 0; k < count; k++) {
                if (!strcmp(piece, dentries[k].name)) {
                    if (dentries[k].type != FS_TYPE_DIR && current_level != destination_level)
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

// TODO: mutexes
int fs_get_inode(char* path, inode_t** inode) {
    *inode = kmalloc(sizeof(inode_t));

    int ret = fs_get_inode_internal(path,*inode);
    if (ret < 0) {
        kfree(*inode);
        return ret;
    }
    klist_set(&((*inode)->mount->inode_cache), (*inode)->id,*inode);

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
                    printf("fs: '%s' worked on /dev/%s, mounted it on %s\n", fssys->name, dev, new_mount->mount_path);
                else
                    printf("fs: '%s' worked without a device, mounted it on %s\n", fssys->name, new_mount->mount_path);

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
    if (path == NULL || strlen(path) == 0)
        return ERR_INV_ARGUMENTS;

    int mnt_len;

    int len     = strlen(path);
    int mnt_id  = -1;
    int longest = -1;

    char* mangleable = kmalloc(len + 1);
    translate_path(mangleable, NULL, path);
    path = mangleable;

    if (path[len - 1] == '/')
        --len;

    klist_entry_t* klist_entry = NULL;
    struct filesystem_mount* fsmnt;
    
    while (true) {
        fsmnt = klist_iter(&mounts, &klist_entry);
        if (fsmnt == NULL)
            break;

        mnt_len = strlen(fsmnt->mount_path) - 1;
        if (mnt_len == 0)
            mnt_len = 1;

        if (memcmp(fsmnt->mount_path, path, mnt_len) == 0 && mnt_len > longest) {
            longest = mnt_len;
            mnt_id  = fsmnt->id;
        }
    }

    if (longest != -1)
        strcpy(new_path, path + longest);

    kfree(mangleable);

    if (mnt_id == -1)
        return FS_ERR_NOT_FOUND;

    *mount = klist_get(&mounts, mnt_id);

    return 0;
}

int fs_count(char* path) {
    inode_t* inode = NULL;

    int ret = fs_get_inode(path, &inode);
    if (ret < 0)
        return ret;

    if (inode->type != FS_TYPE_DIR)
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

    if (inode->type != FS_TYPE_DIR)
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

            dentries[ret].type = FS_TYPE_DIR;
            dentries[ret].inode_id = 1;

            ++ret;
        }
    }
    fs_retire_inode(inode);
    return ret;
}

bool fs_exists(char* path) {
    inode_t* inode = NULL;

    int ret = fs_get_inode(path, &inode);
    fs_retire_inode(inode);

    return ret >= 0;
}

int fs_info(char* path, finfo_t* finfo) {
    inode_t* inode = NULL;

    int ret = fs_get_inode(path, &inode);
    if (ret < 0)
        return ret;

    finfo->type  = inode->type;
    finfo->inode = inode->id;

    fs_retire_inode(inode);
    return 0;
}