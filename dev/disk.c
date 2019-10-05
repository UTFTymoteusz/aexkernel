#pragma once

#include "aex/kmem.h"

#include "dev/dev.h"

enum disk_flags {
    DISK_PARTITIONABLE = 0x0001,
    DISK_BOOTED_FROM   = 0x0002,
};

struct dev_disk {
    int internal_id;
    bool initialized;

    char model_name[64];

    uint16_t flags;
    uint16_t max_sectors_at_once;

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
        return DEV_ERR_NOT_FOUND;

    dev_disk_t* disk = dev->type_specific;

    if (disk->proxy_to != NULL)
        disk = disk->proxy_to;

    if (disk->initialized)
        return ERR_ALREADY_DONE;

    disk->initialized = true;

    if (!(disk->disk_ops->init))
        return ERR_NOT_POSSIBLE;

    disk->disk_ops->init(disk->internal_id);

    return 0;
}

int dev_disk_read(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer) {
    dev_t* dev = dev_disk_get(dev_id);
    if (dev == NULL)
        return DEV_ERR_NOT_FOUND;

    dev_disk_t* disk = dev->type_specific;

    if (disk->proxy_to != NULL) {
        sector += disk->proxy_offset;
        disk = disk->proxy_to;
    }
    if (!disk->initialized)
        dev_disk_init(dev_id);

    if (!(disk->disk_ops->read))
        return ERR_NOT_POSSIBLE;

    uint32_t offset = 0;
    uint32_t sector_size = disk->sector_size;

    // This needs to be improved later down the line
    if (sector_size != 512) {
        uint64_t bytes_remaining = count * 512;
        int ret = 1;

        offset = (sector * 512) % sector_size;
        sector = (sector * 512) / sector_size;
        count  = (count * 512 + (sector_size - 1)) / sector_size;

        if (offset > 0) {
            void* bounce_buffer = kmalloc(sector_size);

            ret = disk->disk_ops->read(disk->internal_id, sector, 1, bounce_buffer);
            if (ret < 0)
                return ret;

            bytes_remaining -= (sector_size - offset);
            memcpy(buffer, (void*)(((size_t)bounce_buffer) + offset), sector_size - offset);

            buffer += sector_size - offset;

            kfree(bounce_buffer);
        }
        int partial_count = 0;

        while (bytes_remaining >= sector_size) {
            bytes_remaining -= sector_size;
            ++partial_count;
        }
        int32_t count2 = count;
        int16_t msat  = disk->max_sectors_at_once;
        void* buffer2 = buffer;

        while (count2 > 0) {
            ret = disk->disk_ops->read(disk->internal_id, sector, (count2 > msat) ? msat : count2, buffer);
            if (ret < -1)
                return ret;

            buffer2 += disk->sector_size * msat;
            sector  += msat;
            count2  -= msat;
        }

        if (bytes_remaining == 0 || ret < 0)
            return ret;

        buffer += partial_count * sector_size;

        void* bounce_buffer = kmalloc(sector_size);

        ret = disk->disk_ops->read(disk->internal_id, sector, 1, bounce_buffer);
        memcpy(buffer, bounce_buffer, bytes_remaining);

        kfree(bounce_buffer);
        return ret;
    }
    return disk->disk_ops->read(disk->internal_id, sector, count, buffer);
}

int dev_disk_dread(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer) {
    dev_t* dev = dev_disk_get(dev_id);
    if (dev == NULL)
        return DEV_ERR_NOT_FOUND;

    dev_disk_t* disk = dev->type_specific;

    if (disk->proxy_to != NULL) {
        sector += disk->proxy_offset;
        disk = disk->proxy_to;
    }
    if (!disk->initialized)
        dev_disk_init(dev_id);

    if (!(disk->disk_ops->read))
        return ERR_NOT_POSSIBLE;

    int ret;
    int32_t count2 = count;
    int16_t msat = disk->max_sectors_at_once;

    while (count2 > 0) {
        ret = disk->disk_ops->read(disk->internal_id, sector, (count2 > msat) ? msat : count2, buffer);
        if (ret < -1)
            return ret;

        buffer += disk->sector_size * msat;
        count2 -= msat;
        sector += msat;
    }
    return 0;
}

int dev_disk_write(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer) {
    dev_t* dev = dev_disk_get(dev_id);
    if (dev == NULL)
        return DEV_ERR_NOT_FOUND;

    dev_disk_t* disk = dev->type_specific;

    if (disk->proxy_to != NULL) {
        sector += disk->proxy_offset;
        disk = disk->proxy_to;
    }
    if (!disk->initialized)
        dev_disk_init(dev_id);

    if (!(disk->disk_ops->write))
        return ERR_NOT_POSSIBLE;

    return disk->disk_ops->write(disk->internal_id, sector, count, buffer);
}

int dev_disk_dwrite(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer) {
    dev_t* dev = dev_disk_get(dev_id);
    if (dev == NULL)
        return DEV_ERR_NOT_FOUND;

    dev_disk_t* disk = dev->type_specific;

    if (disk->proxy_to != NULL) {
        sector += disk->proxy_offset;
        disk = disk->proxy_to;
    }
    if (!disk->initialized)
        dev_disk_init(dev_id);

    if (!(disk->disk_ops->write))
        return ERR_NOT_POSSIBLE;

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
