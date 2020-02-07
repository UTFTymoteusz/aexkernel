#include "aex/rcparray.h"
#include "aex/rcode.h"
#include "aex/string.h"

#include "aex/dev/name.h"
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

void fs_init() {
    printk(PRINTK_INIT "fs: Registering initial filesystems...\n");

    iso9660_init();
    devfs_init();

    printk("fs: Registered filesystems:\n");

    filesystem_t* fs;
    size_t fs_id;
    bool done = false;

    rcparray_foreach(filesystems, fs_id, fs, done)
        printk("  %s\n", fs->name);
    
    printk("\n");
}

void fs_register(filesystem_t* fs) {
    rcparray_add(filesystems, *fs);
    printk(PRINTK_OK "Registered filesystem '%s'\n", fs->name);
}

static int fs_get_mount(char* path, char** new_path) {
    int sel_id   = ERR_NOT_FOUND;
    int spec_len = 0;
    
    mount_t* mount;
    size_t mnt_id;
    bool done = false;
    
    rcparray_foreach(mounts, mnt_id, mount, done) {
        int path_len = strlen(mount->path) - 1;
        
        if ((memcmp(mount->path, path, path_len) == 0) && path_len >= spec_len) {
            sel_id   = mnt_id;
            spec_len = path_len;
        }
    }
    RETURN_IF_ERROR(sel_id);

    if (path[spec_len] != '/')
        *new_path = "/";
    else
        *new_path = path + spec_len;

    return sel_id;
}

int fs_mount(char* dev, char* path, char* type) {
    int dev_id = -1;
    
    if (dev != NULL) {
        dev_id = dev_name2id(dev);
        IF_ERROR(dev_id)
            return dev_id;
    }
    mount_t mount = {0};
    mount.dev_id = dev_id;

    int ret = FS_ERR_NO_MATCHING_FILESYSTEM;

    filesystem_t* fs;
    size_t fs_id;
    bool done = false;

    rcparray_foreach(filesystems, fs_id, fs, done) {
        if (type == NULL || strcmp(fs->name, type) == 0) {
            ret = fs->mount(&mount);
            IF_ERROR(ret)
                continue;

            mount.path = kmalloc(strlen(path) + 3);
            mount.fs = fs;

            int len = strlen(path);
            if (len == 0)
                path = "/";

            if (path[len - 1] != '/')
                snprintf(mount.path, len + 2, "%s/", path);
            else
                snprintf(mount.path, len + 2, "%s", path);

            done = true;
            continue; 
        }
    }
    IF_ERROR(ret)
        return ret;

    ret = rcparray_add(mounts, mount);
    return ret;
}


int64_t _fs_read (int fd, void* buffer, uint64_t len);
int64_t _fs_write(int fd, void* buffer, uint64_t len);
int64_t _fs_seek(int fd, int64_t offset, int mode);
int _fs_close(int fd);

int64_t _fs_ioctl(int fd, int64_t code, void* data);

int _fs_readdir(int fd, dentry_t* dentry_dst);


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
    fd.ops.read  = _fs_read;
    fd.ops.write = _fs_write;
    fd.ops.seek  = _fs_seek;
    fd.ops.close = _fs_close;
    fd.ops.ioctl = _fs_ioctl;
    fd.ops.readdir = _fs_readdir;

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
        return ERR_INV_ARGUMENTS;

    int64_t ret = handle->mount->fs->read(handle->mount, 
                    handle->ifd, buffer, len);

    rcparray_unref(handles, fd);
    return ret;
}

int64_t _fs_write(int fd, void* buffer, uint64_t len) {
    //printk("_fs_write(%i, %lu)\n", fd, len);

    fs_handle_t* handle = rcparray_get(handles, fd);
    if (handle == NULL)
        return ERR_INV_ARGUMENTS;

    int64_t ret = handle->mount->fs->write(handle->mount, 
                    handle->ifd, buffer, len);

    rcparray_unref(handles, fd);
    return ret;
}

int64_t _fs_seek(int fd, int64_t offset, int mode) {
    //printk("_fs_seek(%i, %li, %i)\n", fd, offset, mode);

    fs_handle_t* handle = rcparray_get(handles, fd);
    if (handle == NULL)
        return ERR_INV_ARGUMENTS;

    int64_t ret = handle->mount->fs->seek(handle->mount, 
                    handle->ifd, offset, mode);

    rcparray_unref(handles, fd);
    return ret;
}

int _fs_close(int fd) {
    printk("_fs_close(%i)\n", fd);

    fs_handle_t* handle = rcparray_get(handles, fd);
    if (handle == NULL)
        return ERR_INV_ARGUMENTS;

    fs_handle_t copy = *handle;
        
    rcparray_unref (handles, fd);
    rcparray_remove(handles, fd);
    
    return copy.mount->fs->close(copy.mount, copy.ifd);
}

int64_t _fs_ioctl(int fd, int64_t code, void* data) {
    //printk("_fs_read(%i, %lu)\n", fd, len);

    fs_handle_t* handle = rcparray_get(handles, fd);
    if (handle == NULL)
        return ERR_INV_ARGUMENTS;

    int64_t ret = handle->mount->fs->ioctl(handle->mount, 
                    handle->ifd, code, data);

    rcparray_unref(handles, fd);
    return ret;
}

int _fs_readdir(int fd, dentry_t* dentry_dst) {
    //printk("_fs_readdir(%i)\n", fd);

    fs_handle_t* handle = rcparray_get(handles, fd);
    if (handle == NULL)
        return ERR_INV_ARGUMENTS;

    int64_t ret = handle->mount->fs->readdir(handle->mount, handle->ifd, 
                                        dentry_dst);

    rcparray_unref(handles, fd);
    return ret;
}