#include "aex/kernel.h"
#include "aex/rcode.h"
#include "aex/rcparray.h"
#include "aex/string.h"

#include "aex/dev/char.h"
#include "aex/dev/dev.h"
#include "aex/dev/name.h"

#include "aex/fs/fs.h"

extern rcparray(dev_t*) dev_array;

int devfs_mount(mount_t* mount);
int devfs_open (mount_t* mount, char* path, int mode);
int64_t devfs_read (mount_t* mount, int ifd, void* buffer, uint64_t len);
int64_t devfs_write(mount_t* mount, int ifd, void* buffer, uint64_t len);
int64_t devfs_seek (mount_t* mount, int ifd, int64_t offset, int mode);
int devfs_close(mount_t* mount, int ifd);

int64_t devfs_ioctl(mount_t* mount, int ifd, int64_t code, void* mem);

int devfs_opendir(mount_t* mount, char* path);
int devfs_readdir(mount_t* mount, int ifd, dentry_t* dentry_dst);

int devfs_finfo(mount_t* mount, char* path, finfo_t* finfo);

int devfs_dup(mount_t* mount, int ifd);

filesystem_t devfs = {
    .name = "devfs",
    .mount = devfs_mount,

    .open  = devfs_open ,
    .read  = devfs_read ,
    .write = devfs_write,
    .seek  = devfs_seek ,
    .close = devfs_close,

    .ioctl = devfs_ioctl,
    
    .finfo = devfs_finfo,

    .opendir = devfs_opendir,
    .readdir = devfs_readdir,

    .dup   = devfs_dup,
};

struct lhandle {
    dev_fd_t dev_fd;

    void* data;
    
    int id;
    int refs;
    int type;
};
typedef struct lhandle lhandle_t;

struct devfs_dir {
    int pos;
};
typedef struct devfs_dir devfs_dir_t;

struct devfs_private {
    rcparray(lhandle_t) lhandles;
};
typedef struct devfs_private devfs_private_t;

void devfs_init() {
    fs_register(&devfs);
}

int devfs_mount(mount_t* mount) {
    mount->private_data = kzmalloc(sizeof(devfs_private_t));
    if (mount->dev_id != -1)
        return -1;

    printk(PRINTK_OK "devfs: Mounted\n");
    return 0;
}

// find a place for a spinlock
int devfs_open(mount_t* mount, char* path, int mode) {
    //printk("devfs: open(%s, %i)\n", path, mode);

    devfs_private_t* data = mount->private_data;
    
    int dev_id = dev_name2id(path + 1);
    IF_ERROR(dev_id) {
        if (dev_id == -ENODEV)
            return -ENOENT;
            
        return dev_id;
    }

    lhandle_t handle = {0};
    handle.id     = dev_id;
    handle.type   = 0;
    handle.refs   = 1;

    int id = rcparray_add(data->lhandles, handle);
    lhandle_t* _lhandle = rcparray_get(data->lhandles, id);
    
    int ret = dev_char_open(dev_id, &_lhandle->dev_fd);
    rcparray_unref(data->lhandles, id);

    RETURN_IF_ERROR(ret);

    return id;
}

int64_t devfs_read(mount_t* mount, int ifd, void* buffer, uint64_t len) {
    devfs_private_t* data = mount->private_data;

    lhandle_t* handle = rcparray_get(data->lhandles, ifd);
    if (handle == NULL)
        return -EBADF;

    int ret = dev_char_read(handle->id, &handle->dev_fd, buffer, len);
    rcparray_unref(data->lhandles, ifd);

    return ret;
}

int64_t devfs_write(mount_t* mount, int ifd, void* buffer, uint64_t len) {
    devfs_private_t* data = mount->private_data;

    lhandle_t* handle = rcparray_get(data->lhandles, ifd);
    if (handle == NULL)
        return -EBADF;

    int ret = dev_char_write(handle->id, &handle->dev_fd, buffer, len);
    rcparray_unref(data->lhandles, ifd);

    return ret;
}

int64_t devfs_seek(mount_t* mount, int ifd, int64_t offset, int mode) {
    return -ENOSYS;
}

int devfs_close(mount_t* mount, int ifd) {
    static spinlock_t close_lock = {0};

    devfs_private_t* data = mount->private_data;

    spinlock_acquire(&close_lock);

    lhandle_t* handle = rcparray_get(data->lhandles, ifd);
    if (handle == NULL) {
        spinlock_release(&close_lock);
        return -EBADF;
    }

    if (__sync_sub_and_fetch(&handle->refs, 1) > 0) {
        rcparray_unref (data->lhandles, ifd);
        spinlock_release(&close_lock);
        return 0;
    }
    rcparray_remove(data->lhandles, ifd);
    spinlock_release(&close_lock);

    int ret = dev_char_close(handle->id, &handle->dev_fd);

    if (handle->data != NULL)
        kfree(handle->data);

    rcparray_unref (data->lhandles, ifd);

    return ret;
}

int64_t devfs_ioctl(mount_t* mount, int ifd, int64_t code, void* mem) {
    devfs_private_t* data = mount->private_data;

    lhandle_t* handle = rcparray_get(data->lhandles, ifd);
    if (handle == NULL)
        return -EBADF;

    int ret = dev_char_ioctl(handle->id, &handle->dev_fd, code, mem);
    rcparray_unref(data->lhandles, ifd);

    return ret;
}

int devfs_finfo(mount_t* mount, char* path, finfo_t* finfo) {
    //printk("devfs: finfo(%s)\n", path);

    if (strcmp(path, "/") == 0) {
        memset(finfo, 0, sizeof(finfo_t));
        finfo->type = FS_TYPE_DIR;

        return 0;
    }

    int dev_id = dev_name2id(path + 1);
    IF_ERROR(dev_id) {
        if (dev_id == -ENODEV)
            return -ENOENT;

        return dev_id;
    }

    int _dev_type = dev_type(dev_id);
    IF_ERROR(_dev_type)
        return _dev_type;

    switch (_dev_type) {
        case DEV_TYPE_BLOCK:
            _dev_type = FS_TYPE_BLOCK;
            break;
        case DEV_TYPE_CHAR:
            _dev_type = FS_TYPE_CHAR;
            break;
        case DEV_TYPE_NET:
            _dev_type = FS_TYPE_NET;
            break;
    }
    memset(finfo, '\0', sizeof(finfo_t));

    finfo->dev_id = 0;
    finfo->type = _dev_type;

    return 0;
}

int devfs_opendir(mount_t* mount, char* path) {
    //printk("devfs: opendir(%s)\n", path);

    if (strcmp(path, "/") != 0)
        return -ENOTDIR;

    devfs_private_t* data = mount->private_data;
    
    lhandle_t handle = {0};
    handle.id     = 0;
    handle.type   = 0;
    handle.refs   = 1;

    int id = rcparray_add(data->lhandles, handle);
    lhandle_t* _lhandle = rcparray_get(data->lhandles, id);
    
    _lhandle->data = kzmalloc(sizeof(devfs_dir_t));

    rcparray_unref(data->lhandles, id);
    return id;
}

int devfs_readdir(mount_t* mount, int ifd, dentry_t* dentry_dst) {
    devfs_private_t* data = mount->private_data;

    lhandle_t* handle = rcparray_get(data->lhandles, ifd);
    if (handle == NULL)
        return -EBADF;

    int ret = -1;
    devfs_dir_t* ddata = handle->data;

    while ((size_t) ddata->pos < dev_array.size) {
        int id = ddata->pos++;

        int cd = dev_id2name(id, dentry_dst->name);
        IF_ERROR(cd)
            continue;

        ret = 0;

        switch (dev_type(id)) {
            case DEV_TYPE_BLOCK:
                dentry_dst->type = FS_TYPE_BLOCK;
                break;
            case DEV_TYPE_CHAR:
                dentry_dst->type = FS_TYPE_CHAR;
                break;
            case DEV_TYPE_NET:
                dentry_dst->type = FS_TYPE_NET;
                break;
        }
        break;
    }
    rcparray_unref(data->lhandles, ifd);

    return ret;
}

int devfs_dup(mount_t* mount, int ifd) {
    //printk("devfs: dup(%i)\n", ifd);

    devfs_private_t* data = mount->private_data;

    lhandle_t* handle = rcparray_get(data->lhandles, ifd);
    if (handle == NULL)
        return -EBADF;

    __sync_add_and_fetch(&handle->refs, 1);

    rcparray_unref(data->lhandles, ifd);
    return ifd;
}