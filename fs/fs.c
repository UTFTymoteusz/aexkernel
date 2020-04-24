#include "aex/hook.h"
#include "aex/rcparray.h"
#include "aex/rcode.h"
#include "aex/string.h"

#include "aex/dev/name.h"
#include "aex/proc/task.h"

#include "aex/fs/fs.h"

struct fs_handle {
    int ifd;
    mount_t* mount;
};
typedef struct fs_handle fs_handle_t;

rcparray(filesystem_t) filesystems = {0};
rcparray(mount_t)      mounts      = {0};
rcparray(fs_handle_t)  handles     = {0};

void iso9660_init();
void devfs_init();

void fs_syscall_init();

static void fs_proc_cleanup(pkill_hook_data_t* data);

void fs_init() {
    printk(PRINTK_INIT "fs: Registering initial filesystems...\n");

    iso9660_init();
    devfs_init();

    printk("fs: Registered filesystems:\n");

    filesystem_t* fs;
    size_t fs_id;

    rcparray_foreach(filesystems, fs_id) {
        fs = rcparray_get(filesystems, fs_id);
        printk("  %s\n", fs->name);

        rcparray_unref(filesystems, fs_id);
    }
    printk("\n");

    fs_syscall_init();

    hook_add(HOOK_PKILL, "fs_pkill_fd_closer", fs_proc_cleanup);
}

static void fs_proc_cleanup(pkill_hook_data_t* data) {
    int lid;
    int* fd;

    rcparray_foreach(data->pstruct->fiddies, lid) {
        fd = rcparray_get(data->pstruct->fiddies, lid);
        fd_close(*fd);
        rcparray_unref(data->pstruct->threads, lid);
    }
}

char* fs_to_absolute_path(char* local, char* base) {
    char* path = NULL;

    if (local[0] == '/' || base == NULL) {
        path = kmalloc(strlen(local));
        strcpy(path, local);
    }
    else {
        path = kmalloc(strlen(local) + strlen(base) + 1);
        snprintf(path, strlen(local) + strlen(base) + 1, "%s%s", base, local);
    }
    char segment[256];
    int left;

    char* _path = kmalloc(strlen(path) + 1);
    char* ptr = _path;

    path_walk(path, segment, left) {
        if (strcmp(segment, "..") == 0) {
            while (*ptr != '/' && ptr != _path)
                --ptr;

            continue;
        }
        else if (strcmp(segment, ".") == 0)
            continue;

        *ptr = '/';
        ptr++;

        strcpy(ptr, segment);
        ptr += strlen(segment);
    }

    if (path[strlen(path) - 1] == '/') {
        *ptr = path[strlen(path) - 1];
        ptr++;

        *ptr = '\0';
    }
    kfree(path);
    return _path;
}

void fs_register(filesystem_t* fs) {
    rcparray_add(filesystems, *fs);
    printk(PRINTK_OK "Registered filesystem '%s'\n", fs->name);
}

static int fs_get_mount(char* path, char** new_path) {
    int sel_id   = -ENOENT;
    int spec_len = 0;
    
    mount_t* mount;
    size_t mnt_id;
    
    rcparray_foreach(mounts, mnt_id) {
        mount = rcparray_get(mounts, mnt_id);

        int path_len = strlen(mount->path) - 1;
        
        if ((memcmp(mount->path, path, path_len) == 0) && path_len >= spec_len) {
            sel_id   = mnt_id;
            spec_len = path_len;
        }
        rcparray_unref(mounts, mnt_id);
    }
    RETURN_IF_ERROR(sel_id);

    if (path[spec_len] != '/')
        *new_path = "/";
    else
        *new_path = path + spec_len;

    return sel_id;
}

int fs_mount(char* dev, char* path, char* fsname) {
    int dev_id = -1;
    
    if (dev != NULL) {
        dev_id = dev_name2id(dev);
        IF_ERROR(dev_id)
            return dev_id;
    }
    mount_t mount = {0};
    mount.dev_id = dev_id;

    int ret = -EINVAL;

    size_t fs_id;
    filesystem_t* fs;

    if (strcmp(path, "/") != 0) {
        finfo_t finfo_tmp;
        int fcode = fs_finfo(path, &finfo_tmp);

        RETURN_IF_ERROR(fcode);
    }

    rcparray_foreach(filesystems, fs_id) {
        fs = rcparray_get(filesystems, fs_id);

        if (fsname == NULL || strcmp(fs->name, fsname) == 0) {
            ret = fs->mount(&mount);
            IF_ERROR(ret) {
                rcparray_unref(filesystems, fs_id);
                continue;
            }

            mount.path = kmalloc(strlen(path) + 3);
            mount.fs = fs;

            int len = strlen(path);
            if (len == 0)
                path = "/";

            if (path[len - 1] != '/')
                snprintf(mount.path, len + 2, "%s/", path);
            else
                snprintf(mount.path, len + 2, "%s", path);

            rcparray_unref(filesystems, fs_id);
            break;
        }
    }
    IF_ERROR(ret)
        return -ENOENT;

    ret = rcparray_add(mounts, mount);
    return ret;
}


int64_t _fs_read (int fd, void* buffer, uint64_t len);
int64_t _fs_write(int fd, void* buffer, uint64_t len);
int64_t _fs_seek(int fd, int64_t offset, int mode);
int     _fs_close(int fd);

int64_t _fs_ioctl(int fd, int64_t code, void* data);

int _fs_readdir(int fd, dentry_t* dentry_dst);

int _fs_dup(int fd);


int fs_open(char* path, int mode) {
    char* new_path;
    
    int mount_id = fs_get_mount(path, &new_path);
    IF_ERROR(mount_id)
        return mount_id;

    mount_t* mount = rcparray_get(mounts, mount_id);
    int ret = mount->fs->open(mount, new_path, mode);
    rcparray_unref(mounts, mount_id);

    RETURN_IF_ERROR(ret);

    fs_handle_t handle = {0};
    handle.mount = mount;
    handle.ifd   = ret;

    int fs_fd = rcparray_add(handles, handle);

    file_descriptor_t fd = {0};
    fd.ifd = fs_fd;
    fd.ops.read    = _fs_read;
    fd.ops.write   = _fs_write;
    fd.ops.seek    = _fs_seek;
    fd.ops.close   = _fs_close;
    fd.ops.ioctl   = _fs_ioctl;
    fd.ops.readdir = _fs_readdir;
    fd.ops.dup     = _fs_dup;

    return fd_add(fd);
}

int fs_finfo(char* path, finfo_t* finfo) {
    char* new_path;
    
    int mount_id = fs_get_mount(path, &new_path);
    IF_ERROR(mount_id)
        return mount_id;

    mount_t* mount = rcparray_get(mounts, mount_id);
    int ret = mount->fs->finfo(mount, new_path, finfo);
    rcparray_unref(mounts, mount_id);

    RETURN_IF_ERROR(ret);
    return 0;
}

bool fs_exists(char* path) {
    finfo_t finfo;
    return fs_finfo(path, &finfo) == 0;
}

int fs_opendir(char* path) {
    char* new_path;
    
    int mount_id = fs_get_mount(path, &new_path);
    IF_ERROR(mount_id)
        return mount_id;

    mount_t* mount = rcparray_get(mounts, mount_id);
    int ret = mount->fs->opendir(mount, new_path);
    rcparray_unref(mounts, mount_id);

    RETURN_IF_ERROR(ret);

    fs_handle_t handle = {0};
    handle.mount = mount;
    handle.ifd   = ret;

    int fs_fd = rcparray_add(handles, handle);

    file_descriptor_t fd = {0};
    fd.ifd = fs_fd;
    fd.ops.read  = _fs_read;
    fd.ops.seek  = _fs_seek;
    fd.ops.close = _fs_close;
    fd.ops.ioctl = _fs_ioctl;
    fd.ops.readdir = _fs_readdir;

    return fd_add(fd);
}


int64_t _fs_read(int fd, void* buffer, uint64_t len) {
    //printk("_fs_read(%i, %lu)\n", fd, len);

    fs_handle_t* handle = rcparray_get(handles, fd);
    if (handle == NULL)
        return -EBADF;

    int64_t ret = handle->mount->fs->read(handle->mount, 
                    handle->ifd, buffer, len);

    rcparray_unref(handles, fd);
    return ret;
}

int64_t _fs_write(int fd, void* buffer, uint64_t len) {
    //printk("_fs_write(%i, %lu)\n", fd, len);

    fs_handle_t* handle = rcparray_get(handles, fd);
    if (handle == NULL)
        return -EBADF;

    int64_t ret = handle->mount->fs->write(handle->mount, 
                    handle->ifd, buffer, len);

    rcparray_unref(handles, fd);
    return ret;
}

int64_t _fs_seek(int fd, int64_t offset, int mode) {
    //printk("_fs_seek(%i, %li, %i)\n", fd, offset, mode);

    fs_handle_t* handle = rcparray_get(handles, fd);
    if (handle == NULL)
        return -EBADF;

    int64_t ret = handle->mount->fs->seek(handle->mount, 
                    handle->ifd, offset, mode);

    rcparray_unref(handles, fd);
    return ret;
}

int _fs_close(int fd) {
    //printk("_fs_close(%i)\n", fd);

    fs_handle_t* handle = rcparray_get(handles, fd);
    if (handle == NULL)
        return -EBADF;

    fs_handle_t copy = *handle;
        
    rcparray_unref (handles, fd);
    rcparray_remove(handles, fd);
    
    return copy.mount->fs->close(copy.mount, copy.ifd);
}

int64_t _fs_ioctl(int fd, int64_t code, void* data) {
    //printk("_fs_read(%i, %lu)\n", fd, len);

    fs_handle_t* handle = rcparray_get(handles, fd);
    if (handle == NULL)
        return -EBADF;

    int64_t ret = handle->mount->fs->ioctl(handle->mount, 
                    handle->ifd, code, data);

    rcparray_unref(handles, fd);
    return ret;
}

int _fs_readdir(int fd, dentry_t* dentry_dst) {
    //printk("_fs_readdir(%i)\n", fd);

    fs_handle_t* handle = rcparray_get(handles, fd);
    if (handle == NULL)
        return -EBADF;

    int64_t ret = handle->mount->fs->readdir(handle->mount, handle->ifd, 
                                        dentry_dst);

    rcparray_unref(handles, fd);
    return ret;
}

int _fs_dup(int fd) {
    fs_handle_t* handle = rcparray_get(handles, fd);
    if (handle == NULL)
        return -EBADF;

    int ret = handle->mount->fs->dup(handle->mount, handle->ifd);
    IF_ERROR(ret) {
        rcparray_unref(handles, fd);
        return ret;
    }

    fs_handle_t handle_new = {0};
    handle_new.mount = handle->mount;
    handle_new.ifd   = handle->ifd;

    int fd_new = rcparray_add(handles, handle_new);

    rcparray_unref(handles, fd);
    return fd_new;
}