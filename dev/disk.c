#pragma once

#include "aex/kmem.h"

#include "dev/dev.h"

struct dev_disk {
    int internal_id;
    bool initialized;

    char model_name[64];

    uint32_t total_sectors;
    uint32_t sector_size;
    
    struct dev_disk_ops* disk_ops;

    struct dev_disk* proxy_to;
    uint64_t proxy_offset;
};
typedef struct dev_disk dev_disk_t;

struct dev_disk_ops {
    int (*init) (int drive);
    int (*read) (int drive, uint64_t sector, uint16_t count, uint8_t* buffer);
    int (*write)(int drive, uint64_t sector, uint16_t count, uint8_t* buffer);
    void (*release)(int drive);

    long (*ioctl)(int drive, long, long);
};

int dev_register_disk(char* name, struct dev_disk* disk) {

    dev_t* d = kmalloc(sizeof(dev_t));
    d->type = DEV_TYPE_DISK;
    d->name = name;
    d->type_specific = (void*)disk;

    int ret = dev_register(d);

    if (ret >= 0) {
        //printf("Registered disk /dev/%s\n", name);
        // Extra stuff here for later
    }

    return ret;
}
void dev_unregister_disk(char* name) {
    name = name; // temp
    kpanic("dev_unregister_disk() is unimplemented");
}

inline dev_t* dev_disk_get(int dev_id) {
    
    dev_t* dev = dev_array[dev_id];

    if (dev == NULL)
        return NULL;
    if (dev->type != DEV_TYPE_DISK)
        return NULL;

    return dev;
}

int dev_disk_init(int dev_id) {

    dev_t* dev = dev_disk_get(dev_id);
    if (dev == NULL)
        return -1;
        
    dev_disk_t* disk = dev->type_specific;

    if (disk->proxy_to != NULL)
        disk = disk->proxy_to;

    if (disk->initialized)
        return -1;

    disk->initialized = true;

    if (!(disk->disk_ops->init))
        return -1;

    disk->disk_ops->init(disk->internal_id);

    return 0;
}

int dev_disk_read(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer) {

    dev_t* dev = dev_disk_get(dev_id);
    if (dev == NULL)
        return -1;

    dev_disk_t* disk = dev->type_specific;

    if (disk->proxy_to != NULL) {
        sector += disk->proxy_offset;
        disk = disk->proxy_to;
    }

    if (!disk->initialized)
        dev_disk_init(dev_id);

    if (!(disk->disk_ops->read))
        return -1;

    return disk->disk_ops->read(disk->internal_id, sector, count, buffer);
}
int dev_disk_write(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer) {

    dev_t* dev = dev_disk_get(dev_id);
    if (dev == NULL)
        return -1;

    dev_disk_t* disk = dev->type_specific;

    if (disk->proxy_to != NULL) {
        sector += disk->proxy_offset;
        disk = disk->proxy_to;
    }

    if (!disk->initialized)
        dev_disk_init(dev_id);

    if (!(disk->disk_ops->read))
        return -1;

    return disk->disk_ops->write(disk->internal_id, sector, count, buffer);
}

void dev_disk_release(int dev_id) {

    dev_t* dev = dev_disk_get(dev_id);
    if (dev == NULL)
        return;
        
    dev_disk_t* disk = dev->type_specific;

    if (disk->proxy_to != NULL)
        disk = disk->proxy_to;
    
    if (!(disk->initialized))
        return;

    disk->initialized = false;

    if (!(disk->disk_ops->release))
        return;

    disk->disk_ops->release(disk->internal_id);
}

dev_disk_t* dev_disk_get_data(int dev_id) {

    dev_t* dev = dev_disk_get(dev_id);
    if (dev == NULL)
        return NULL;

    dev_disk_t* disk = dev->type_specific;
    
    if (disk->proxy_to != NULL)
        disk = disk->proxy_to;

    return disk;
}
