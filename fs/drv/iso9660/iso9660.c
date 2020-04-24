#include "aex/dev/block.h"
#include "aex/fs/fs.h"

#include "aex/rcparray.h"
#include "aex/math.h"
#include "aex/mem.h"
#include "aex/rcode.h"

#include "iso9660types.h"

#define BLOCK_SIZE 2048

int iso9660_mount(mount_t* mount);
int iso9660_open (mount_t* mount, char* path, int mode);
int64_t iso9660_read (mount_t* mount, int ifd, void* buffer, uint64_t len);
int64_t iso9660_write(mount_t* mount, int ifd, void* buffer, uint64_t len);
int64_t iso9660_seek (mount_t* mount, int ifd, int64_t offset, int mode);
int iso9660_close(mount_t* mount, int ifd);

int64_t iso9660_ioctl(mount_t* mount, int fd, int64_t code, void* ioctl_data);

int iso9660_opendir(mount_t* mount, char* path);
int iso9660_readdir(mount_t* mount, int ifd, dentry_t* dentry_dst);

int iso9660_finfo (mount_t* mount, char* path, finfo_t* finfo);

filesystem_t iso9660_fs = {
    .name = "iso9660",
    .mount = iso9660_mount,

    .open  = iso9660_open ,
    .read  = iso9660_read ,
    .write = iso9660_write,
    .seek  = iso9660_seek ,
    .close = iso9660_close,
    .ioctl = iso9660_ioctl,

    .opendir = iso9660_opendir,
    .readdir = iso9660_readdir,

    .finfo = iso9660_finfo,
};

struct lhandle {
    uint32_t start;

    uint32_t size;
    uint32_t pos;

    uint32_t block_read;

    void* buffer;

    int refs;
    int type;
};
typedef struct lhandle lhandle_t;

struct iso9660private {
    struct iso9660_primary_volume_desc* pvd;
    uint32_t root_lba;

    rcparray(lhandle_t) lhandles;
};
typedef struct iso9660private iso9660private_t;

spinlock_t iso9660_lock = create_spinlock();

void iso9660_init() {
    fs_register(&iso9660_fs);
}

int iso9660_mount(mount_t* mount) {
    iso9660private_t* private_data = kzmalloc(sizeof(iso9660private_t));
    mount->private_data = private_data;

    int offset = 0;
    CLEANUP void* yeet = kmalloc(2048);
    bool complete = false;
    struct iso9660_primary_volume_desc* pvd = kmalloc(sizeof(struct iso9660_primary_volume_desc));

    memset(pvd, 0, sizeof(struct iso9660_primary_volume_desc));

    int ret;

    while (true) {
        if (offset > 24) {
            printk(PRINTK_WARN "iso9660: Mount failed: Too many volume descriptors\n");

            kfree(pvd);
            return -1;
        }
        ret = dev_block_read(mount->dev_id, yeet, (64 + (offset++ * 4)) * 512, BLOCK_SIZE);
        RETURN_IF_ERROR(ret);

        struct iso9660_vdesc* a = yeet;

        if (a->type == ISO9660_PRIMARY_VOLUME) {
            complete = true;
            memcpy(pvd, a, sizeof(struct iso9660_primary_volume_desc));
        }
        if (a->type == ISO9660_TERMINATOR)
            break;

        if (memcmp(a->identifier, "CD001", 5))
            continue;
    }
    if (!complete) {
        kfree(pvd);
        printk(PRINTK_WARN "iso9660: Mount failed: Primary Volume Descriptor not found\n");

        return -1;
    }
    private_data->pvd = pvd;
    private_data->root_lba = pvd->root.data_lba.le;

    ret = dev_block_read(mount->dev_id, yeet, pvd->root.data_lba.le * 2048, 2048);
    IF_ERROR(ret)
        return ret;

    printk(PRINTK_OK "iso9660: Mounted\n");
    return 0;
}

static int iso9660_find(int dev, uint32_t* lba, uint32_t* size, char* segment, 
                iso9660_dentry_t* dentry_ext) {

    uint64_t start = *lba * BLOCK_SIZE;
    uint32_t progress = 0;

    CLEANUP uint8_t* buffer = kmalloc(BLOCK_SIZE);
    uint16_t offset = 0;

    dev_block_read(dev, buffer, start, BLOCK_SIZE);

    char namebuffer[256];

    while (progress < *size) {
        iso9660_dentry_t* dentry = (void*) buffer + offset;

        if (dentry->len == 0) {
            progress += BLOCK_SIZE;
            dev_block_read(dev, buffer, start + progress, BLOCK_SIZE);

            if (progress >= *size)
                break;

            offset = 0;
            continue;
        }
        offset += dentry->len;

        int filename_len = dentry->filename_len;

        if (dentry->filename[0] == '\0') {
            strcpy(namebuffer, ".");
            filename_len = 2;
        }
        else if (dentry->filename[0] == '\1') {
            strcpy(namebuffer, "..");
            filename_len = 3;
        }
        else {
            snprintf(namebuffer, min(256, filename_len + 1), 
                                        "%s", dentry->filename);

            *(strchrnul(namebuffer, ';')) = '\0';
        }

        if (strcmp(namebuffer, segment) == 0) {
            *lba  = dentry->data_lba.le;
            *size = dentry->data_len.le;

            if (dentry_ext != NULL)
                memcpy(dentry_ext, dentry, sizeof(*dentry_ext));

            return (dentry->flags & 0x02) ? FS_TYPE_DIR : FS_TYPE_REG;
        }

        if (offset >= BLOCK_SIZE) {
            progress += BLOCK_SIZE;
            dev_block_read(dev, buffer, start + progress, BLOCK_SIZE);

            if (progress >= *size)
                break;

            offset = 0;
            continue;
        }
    }
    return FS_TYPE_NONE;
}

int iso9660_open(mount_t* mount, char* path, int mode) {
    //printk("iso9660: open(%s, %i)\n", path, mode);

    iso9660private_t* data = mount->private_data;
    uint32_t dir  = data->root_lba;
    uint32_t size = data->pvd->root.data_len.le;

    char segment[256];
    int  left;
    int  type;

    path_walk(path, segment, left) {
        type = iso9660_find(mount->dev_id, &dir, &size, segment, NULL);
        if (type == FS_TYPE_NONE)
            return -ENOENT;

        if (left == 0)
            break;

        if (type != FS_TYPE_DIR)
            return -ENOENT;
    }
    if (type == FS_TYPE_DIR)
        return -EISDIR;

    spinlock_acquire(&iso9660_lock);

    lhandle_t handle = {0};
    handle.start = dir;
    handle.size = size;
    handle.type = type;
    handle.refs = 1;

    int ret = rcparray_add(data->lhandles, handle);
    spinlock_release(&iso9660_lock);

    return ret;
}

int64_t iso9660_read(mount_t* mount, int ifd, void* buffer, uint64_t len) {
    //printk("iso9660: read(%i, %lu)\n", ifd, len);

    iso9660private_t* data = mount->private_data;
    lhandle_t* handle = rcparray_get(data->lhandles, ifd);
    if (handle == NULL)
        return -EBADF;

    if (handle->type == FS_TYPE_DIR) {
        rcparray_unref(data->lhandles, ifd);
        return -EBADF;
    }

    //printk("start %i, pos %i, size %i\n", handle->start, handle->pos, handle->size);

    int actual_len = min(handle->size - handle->pos, (size_t) len);
    
    dev_block_read(mount->dev_id, buffer, 
        (uint64_t) handle->start * BLOCK_SIZE + handle->pos, actual_len);
    handle->pos += actual_len;

    //printk("start %i, pos %i, size %i\n", handle->start, handle->pos, handle->size);

    rcparray_unref(data->lhandles, ifd);
    return actual_len;
}

int64_t iso9660_write(UNUSED mount_t* mount, UNUSED int ifd, 
                    UNUSED void* buffer, UNUSED uint64_t len) {
    return -EROFS;
}

int64_t iso9660_seek(mount_t* mount, int ifd, int64_t offset, int mode) {
    //printk("iso9660: seek(%i, %li, %i)\n", ifd, offset, mode);

    iso9660private_t* data = mount->private_data;
    lhandle_t* handle = rcparray_get(data->lhandles, ifd);
    if (handle == NULL)
        return -EBADF;

    if (handle->type == FS_TYPE_DIR) {
        rcparray_unref(data->lhandles, ifd);
        return -EBADF;
    }

    int64_t ret = 0;
    int64_t new_pos = 0;

    switch (mode) {
        case 0:
            new_pos = offset;
            break;
        case 1:
            new_pos = (int32_t) handle->pos + offset;
            break;
        case 2:
            new_pos = (int32_t) handle->size + offset;
            break;
    }
    handle->pos = min(max(new_pos, 0), handle->size);
    //printk("set: %u\n", handle->pos);

    rcparray_unref(data->lhandles, ifd);
    return ret;
}

int iso9660_close(mount_t* mount, int ifd) {
    //printk("iso9660: close(%i)\n", ifd);
    
    iso9660private_t* data = mount->private_data;
    lhandle_t* handle = rcparray_get(data->lhandles, ifd);
    if (handle == NULL)
        return -EBADF;

    spinlock_acquire(&iso9660_lock);

    int refs = __sync_sub_and_fetch(&handle->refs, 1);
    if (refs == 0)
        rcparray_remove(data->lhandles, ifd);

    rcparray_unref(data->lhandles, ifd);
    spinlock_release(&iso9660_lock);

    return 0;
}

int64_t iso9660_ioctl(mount_t* mount, int ifd, int64_t code, void* ioctl_data) {
    if (code != IOCTL_BYTES_AVAILABLE)
        return -ENOSYS;

    iso9660private_t* data = mount->private_data;
    lhandle_t* handle = rcparray_get(data->lhandles, ifd);
    if (handle == NULL)
        return -EBADF;

    *((long*) ioctl_data) = handle->size - handle->pos;
        
    rcparray_unref(data->lhandles, ifd);

    return 0;
}

int iso9660_opendir(mount_t* mount, char* path) {
    //printk("iso9660: opendir(%s)\n", path);

    iso9660private_t* data = mount->private_data;
    uint32_t dir  = data->root_lba;
    uint32_t size = data->pvd->root.data_len.le;

    char segment[256];
    int  left;
    int  type;

    path_walk(path, segment, left) {
        type = iso9660_find(mount->dev_id, &dir, &size, segment, NULL);
        if (type == FS_TYPE_NONE)
            return -ENOENT;

        if (left == 0)
            break;

        if (type != FS_TYPE_DIR)
            return -ENOENT;
    }
    if (type != FS_TYPE_DIR)
        return -ENOTDIR;

    lhandle_t handle = {0};
    handle.start = dir;
    handle.size  = size;
    handle.type  = type;
    handle.buffer     = kmalloc(BLOCK_SIZE);
    handle.block_read = 0xFFFFFFFF;
    handle.refs = 0;

    dev_block_read(mount->dev_id, handle.buffer, 
        handle.start * BLOCK_SIZE, BLOCK_SIZE);

    spinlock_acquire(&iso9660_lock);
    int ret = rcparray_add(data->lhandles, handle);
    spinlock_release(&iso9660_lock);
    
    return ret;
}

int iso9660_readdir(mount_t* mount, int ifd, dentry_t* dentry_dst) {
    //printk("iso9660: readdir(%i)\n", ifd);

    iso9660private_t* data = mount->private_data;
    lhandle_t* handle = rcparray_get(data->lhandles, ifd);
    if (handle == NULL)
        return -EBADF;

    if (handle->type != FS_TYPE_DIR) {
        rcparray_unref(data->lhandles, ifd);
        return -EBADF;
    }

    char* namebuffer = dentry_dst->name;

    while (true) {
        //printk("%i, %i\n", handle->pos, handle->size);
        if (handle->pos >= handle->size){
            rcparray_unref(data->lhandles, ifd);
            return -1;
        }

        if ((handle->pos / BLOCK_SIZE) != handle->block_read) {
            dev_block_read(mount->dev_id, handle->buffer, 
                handle->start * BLOCK_SIZE + handle->pos, BLOCK_SIZE);
            
            handle->block_read = handle->pos / BLOCK_SIZE;
        }

        iso9660_dentry_t* dentry = handle->buffer + handle->pos % BLOCK_SIZE;
        dentry_dst->type = dentry->flags & 0x02 ? FS_TYPE_DIR : FS_TYPE_REG;

        if (dentry->len == 0) {
            handle->pos += BLOCK_SIZE - (handle->pos % BLOCK_SIZE);
            if (handle->pos >= handle->size) {
                rcparray_unref(data->lhandles, ifd);
                return -1;
            }
            continue;
        }
        handle->pos += dentry->len;

        int filename_len = dentry->filename_len;

        if (dentry->filename[0] == '\0') {
            strcpy(namebuffer, ".");
            filename_len = 2;
        }
        else if (dentry->filename[0] == '\1') {
            strcpy(namebuffer, "..");
            filename_len = 3;
        }
        else {
            snprintf(namebuffer, min(255, filename_len + 1), 
                                        "%s", dentry->filename);

            *(strchrnul(namebuffer, ';')) = '\0';
        }
        break;
    }
    rcparray_unref(data->lhandles, ifd);
    return 0;
}

int iso9660_finfo(mount_t* mount, char* path, finfo_t* finfo) {
    //printk("iso9660: finfo(%s)\n", path);

    iso9660private_t* data = mount->private_data;
    uint32_t dir  = data->root_lba;
    uint32_t size = data->pvd->root.data_len.le;

    char segment[256];
    int  left;
    int  type;

    iso9660_dentry_t dentry = data->pvd->root;

    path_walk(path, segment, left) {
        type = iso9660_find(mount->dev_id, &dir, &size, segment, &dentry);
        if (type == FS_TYPE_NONE)
            return -ENOENT;

        if (left == 0)
            break;

        if (type != FS_TYPE_DIR)
            return -ENOENT;
    }
    finfo->dev_id = mount->dev_id;
    finfo->size   = dentry.data_len.le;
    finfo->block_size = BLOCK_SIZE;
    finfo->block_count = 
        (dentry.data_len.le + BLOCK_SIZE - 1) / BLOCK_SIZE;
    finfo->type = (dentry.flags & 0x02) ? FS_TYPE_DIR : FS_TYPE_REG;

    return 0;
}