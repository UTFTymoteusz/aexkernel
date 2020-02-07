#include "aex/kernel.h"
#include "aex/rcode.h"
#include "aex/rcparray.h"

#include "aex/dev/char.h"
#include "aex/dev/dev.h"
#include "aex/dev/name.h"

#include "aex/fs/fs.h"

int devfs_mount(mount_t* mount);
int devfs_open (mount_t* mount, char* path, int mode);
int64_t devfs_read (mount_t* mount, int ifd, void* buffer, uint64_t len);
int64_t devfs_write(mount_t* mount, int ifd, void* buffer, uint64_t len);
int64_t devfs_seek (mount_t* mount, int ifd, int64_t offset, int mode);
int devfs_close(mount_t* mount, int ifd);

int64_t devfs_ioctl(mount_t* mount, int ifd, int64_t code, void* mem);

filesystem_t devfs = {
    .name = "devfs",
    .mount = devfs_mount,

    .open  = devfs_open ,
    .read  = devfs_read ,
    .write = devfs_write,
    .seek  = devfs_seek ,
    .close = devfs_close,

    .ioctl = devfs_ioctl,
};

struct lhandle {
    dev_fd_t dev_fd;
    
    int id;
    int type;
};
typedef struct lhandle lhandle_t;

struct devfs_private {
    rcparray(lhandle_t) lhandles;
};
typedef struct devfs_private devfs_private_t;

void devfs_init() {
    fs_register(&devfs);
}

int devfs_mount(mount_t* mount) {
    mount->private_data = kzmalloc(sizeof(devfs_private_t));

    printk(PRINTK_OK "devfs: Mounted\n");
    return 0;
}

int devfs_open(mount_t* mount, char* path, int mode) {
    printk("devfs: open(%s, %i)\n", path, mode);

    devfs_private_t* data = mount->private_data;
    
    int dev_id = dev_name2id(path + 1);
    IF_ERROR(dev_id)
        return dev_id;

    lhandle_t handle = {0};
    handle.id     = dev_id;
    handle.type   = 0;

    dev_char_open(dev_id, &handle.dev_fd);
    return rcparray_add(data->lhandles, handle);
}

int64_t devfs_read(mount_t* mount, int ifd, void* buffer, uint64_t len) {
    devfs_private_t* data = mount->private_data;

    lhandle_t* handle = rcparray_get(data->lhandles, ifd);
    if (handle == NULL)
        return ERR_GONE;

    int ret = dev_char_read(handle->id, &handle->dev_fd, buffer, len);
    rcparray_unref(data->lhandles, ifd);

    return ret;
}

int64_t devfs_write(mount_t* mount, int ifd, void* buffer, uint64_t len) {
    devfs_private_t* data = mount->private_data;

    lhandle_t* handle = rcparray_get(data->lhandles, ifd);
    if (handle == NULL)
        return ERR_GONE;

    int ret = dev_char_write(handle->id, &handle->dev_fd, buffer, len);
    rcparray_unref(data->lhandles, ifd);

    return ret;
}

int64_t devfs_seek(mount_t* mount, int ifd, int64_t offset, int mode) {
    return ERR_NOT_IMPLEMENTED;
}

int devfs_close(mount_t* mount, int ifd) {
    devfs_private_t* data = mount->private_data;

    lhandle_t* handle = rcparray_get(data->lhandles, ifd);
    if (handle == NULL)
        return ERR_GONE;

    lhandle_t copy = *handle;

    rcparray_unref (data->lhandles, ifd);
    rcparray_remove(data->lhandles, ifd);

    return dev_char_close(handle->id, &copy.dev_fd);
}

int64_t devfs_ioctl(mount_t* mount, int ifd, int64_t code, void* mem) {
    devfs_private_t* data = mount->private_data;

    lhandle_t* handle = rcparray_get(data->lhandles, ifd);
    if (handle == NULL)
        return ERR_GONE;

    int ret = dev_char_ioctl(handle->id, &handle->dev_fd, code, mem);
    rcparray_unref(data->lhandles, ifd);

    return ret;
}