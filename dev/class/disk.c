#include "aex/mem.h"
#include "aex/rcparray.h"
#include "aex/string.h"

#include "aex/dev/block.h"
#include "aex/dev/tree.h"
#include "aex/dev/class/disk.h"

int class_disk_bind(device_t* device);

rcparray(device_t*) disks;

class_t disk_class = {
    .name = "disk",
    .bind = class_disk_bind,
};

void class_disk_init() {
    dev_register_class(&disk_class);
}


static int class_disk_devinit(int ifd) {
    device_t** _device = rcparray_get(disks, ifd);
    if (_device == NULL)
        return -1;

    device_t* device = *_device;

    class_disk_t* data = device->class_data;
    int ret = data->init(device);

    rcparray_unref(disks, ifd);
    return ret;
}

static int class_disk_devread(int ifd, uint64_t sector, uint16_t count, uint8_t* buffer) {
    device_t** _device = rcparray_get(disks, ifd);
    if (_device == NULL)
        return -1;

    device_t* device = *_device;

    class_disk_t* data = device->class_data;
    int ret = data->read(device, sector, count, buffer);

    rcparray_unref(disks, ifd);
    return ret;
}

static int class_disk_devwrite(int ifd, uint64_t sector, uint16_t count, uint8_t* buffer) {
    device_t** _device = rcparray_get(disks, ifd);
    if (_device == NULL)
        return -1;

    device_t* device = *_device;

    class_disk_t* data = device->class_data;
    int ret = data->read(device, sector, count, buffer);

    rcparray_unref(disks, ifd);
    return ret;
}

static int class_disk_devrelease(int ifd) {
    device_t** _device = rcparray_get(disks, ifd);
    if (_device == NULL)
        return -1;

    device_t* device = *_device;

    class_disk_t* data = device->class_data;
    int ret = data->release(device);

    rcparray_unref(disks, ifd);
    return ret;
}


static int class_bogus_func(UNUSED device_t* dev) {
    printk("bogus called\n");
    return 0;
}

int class_disk_bind(device_t* device) {
    class_disk_t* data = device->class_data;
    int id = rcparray_add(disks, device);

    dev_block_t* blkdev = kmalloc(sizeof(dev_block_t));

    blkdev->internal_id = id;
    blkdev->total_sectors = data->sector_count;
    blkdev->sector_size   = data->sector_size;
    blkdev->burst_max     = data->burst_max;

    dev_block_ops_t* blkops = kmalloc(sizeof(dev_block_ops_t));

    blkops->init  = class_disk_devinit;
    blkops->read  = class_disk_devread;
    blkops->write = class_disk_devwrite;
    blkops->release = class_disk_devrelease;

    blkdev->block_ops = blkops;

    if (data->init == NULL)
        data->init = class_bogus_func;

    if (data->release == NULL)
        data->release = class_bogus_func;

    dev_register_block(data->name, blkdev);

    printk("Class disk tried to bind to '%s' (%s)\n", device->name, data->name);
    return -1;
}


int disk_get_type(char* name) {
    int id = -1;
    device_t** _device = rcparray_find(disks, device_t*, 
        strcmp((((class_disk_t*)((*elem)->class_data))->name), name) == 0, &id);

    if (_device == NULL)
        return DISK_TYPE_INVALID;

    device_t* device = *_device;
    class_disk_t* data = (class_disk_t*)device->class_data;

    int ret = data->type;
    rcparray_unref(disks, id);

    return ret;
}