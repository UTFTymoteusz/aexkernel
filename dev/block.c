#include "aex/kmem.h"
#include "aex/klist.h"
#include "aex/rcode.h"

#include "dev/dev.h"
#include "kernel/sys.h"

#include <string.h>

#include "block.h"

enum block_flags;

struct dev_block;
struct dev_block_ops;

typedef struct dev_block dev_block_t;

struct klist block_devs;

int dev_register_block(char* name, struct dev_block* block_dev) {
    dev_t* d = kmalloc(sizeof(dev_t));
    d->type = DEV_TYPE_BLOCK;
    d->name = name;
    d->type_specific = block_dev;

    int ret = dev_register(d);
    if (ret >= 0) {
        //printf("Registered block /dev/%s\n", name);
        // Extra stuff here for later
    }
    return ret;
}

void dev_unregister_block(char* name) {
    name = name; // temp
    kpanic("dev_unregister_block() is unimplemented");
}

inline dev_t* dev_block_get(int dev_id) {
    dev_t* dev = dev_array[dev_id];

    if (dev == NULL)
        return NULL;
    if (dev->type != DEV_TYPE_BLOCK)
        return NULL;

    return dev;
}

int dev_block_init(int dev_id) {
    dev_t* dev = dev_block_get(dev_id);
    if (dev == NULL)
        return DEV_ERR_NOT_FOUND;

    dev_block_t* block_dev = dev->type_specific;

    if (block_dev->proxy_to != NULL)
        block_dev = block_dev->proxy_to;

    if (block_dev->initialized)
        return ERR_ALREADY_DONE;

    block_dev->initialized = true;

    if (!(block_dev->block_ops->init))
        return ERR_NOT_POSSIBLE;

    block_dev->block_ops->init(block_dev->internal_id);

    return 0;
}

int dev_block_read(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer) {
    dev_t* dev = dev_block_get(dev_id);
    if (dev == NULL)
        return DEV_ERR_NOT_FOUND;

    dev_block_t* block_dev = dev->type_specific;

    if (block_dev->proxy_to != NULL) {
        sector += block_dev->proxy_offset;
        block_dev = block_dev->proxy_to;
    }
    if (!block_dev->initialized)
        dev_block_init(dev_id);

    if (!(block_dev->block_ops->read))
        return ERR_NOT_POSSIBLE;

    uint32_t offset = 0;
    uint32_t sector_size = block_dev->sector_size;

    // This needs to be improved later down the line
    if (sector_size != 512) {
        uint64_t bytes_remaining = count * 512;
        int ret = 1;

        offset = (sector * 512) % sector_size;
        sector = (sector * 512) / sector_size;
        count  = (count * 512 + (sector_size - 1)) / sector_size;

        if (offset > 0) {
            void* bounce_buffer = kmalloc(sector_size);

            ret = block_dev->block_ops->read(block_dev->internal_id, sector, 1, bounce_buffer);
            if (ret < 0)
                return ret;

            bytes_remaining -= (sector_size - offset);
            memcpy(buffer, (void*) (((size_t) bounce_buffer) + offset), sector_size - offset);

            buffer += sector_size - offset;

            kfree(bounce_buffer);
        }
        int partial_count = 0;

        while (bytes_remaining >= sector_size) {
            bytes_remaining -= sector_size;
            ++partial_count;
        }
        int32_t count2 = count;
        int16_t msat  = block_dev->max_sectors_at_once;
        void* buffer2 = buffer;

        while (count2 > 0) {
            ret = block_dev->block_ops->read(block_dev->internal_id, sector, (count2 > msat) ? msat : count2, buffer);
            if (ret < -1)
                return ret;

            buffer2 += block_dev->sector_size * msat;
            sector  += msat;
            count2  -= msat;
        }
        if (bytes_remaining == 0 || ret < 0)
            return ret;

        buffer += partial_count * sector_size;

        void* bounce_buffer = kmalloc(sector_size);

        ret = block_dev->block_ops->read(block_dev->internal_id, sector, 1, bounce_buffer);
        memcpy(buffer, bounce_buffer, bytes_remaining);

        kfree(bounce_buffer);
        return ret;
    }
    return block_dev->block_ops->read(block_dev->internal_id, sector, count, buffer);
}

int dev_block_dread(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer) {
    dev_t* dev = dev_block_get(dev_id);
    if (dev == NULL)
        return DEV_ERR_NOT_FOUND;

    dev_block_t* block_dev = dev->type_specific;

    if (block_dev->proxy_to != NULL) {
        sector += block_dev->proxy_offset;
        block_dev = block_dev->proxy_to;
    }
    if (!block_dev->initialized)
        dev_block_init(dev_id);

    if (!(block_dev->block_ops->read))
        return ERR_NOT_POSSIBLE;

    int32_t count2 = count;
    int16_t msat = block_dev->max_sectors_at_once;

    int ret;

    while (count2 > 0) {
        ret = block_dev->block_ops->read(block_dev->internal_id, sector, (count2 > msat) ? msat : count2, buffer);
        if (ret < -1)
            return ret;

        buffer += block_dev->sector_size * msat;
        count2 -= msat;
        sector += msat;
    }
    return 0;
}

int dev_block_write(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer) {
    dev_t* dev = dev_block_get(dev_id);
    if (dev == NULL)
        return DEV_ERR_NOT_FOUND;

    dev_block_t* block_dev = dev->type_specific;

    if (block_dev->proxy_to != NULL) {
        sector += block_dev->proxy_offset;
        block_dev = block_dev->proxy_to;
    }
    if (!block_dev->initialized)
        dev_block_init(dev_id);

    if (!(block_dev->block_ops->write))
        return ERR_NOT_POSSIBLE;

    return block_dev->block_ops->write(block_dev->internal_id, sector, count, buffer);
}

int dev_block_dwrite(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer) {
    dev_t* dev = dev_block_get(dev_id);
    if (dev == NULL)
        return DEV_ERR_NOT_FOUND;

    dev_block_t* block_dev = dev->type_specific;

    if (block_dev->proxy_to != NULL) {
        sector += block_dev->proxy_offset;
        block_dev = block_dev->proxy_to;
    }
    if (!block_dev->initialized)
        dev_block_init(dev_id);

    if (!(block_dev->block_ops->write))
        return ERR_NOT_POSSIBLE;

    return block_dev->block_ops->write(block_dev->internal_id, sector, count, buffer);
}

void dev_block_release(int dev_id) {
    dev_t* dev = dev_block_get(dev_id);
    if (dev == NULL)
        return;

    dev_block_t* block_dev = dev->type_specific;

    if (block_dev->proxy_to != NULL)
        block_dev = block_dev->proxy_to;

    if (!(block_dev->initialized))
        return;

    block_dev->initialized = false;

    if (!(block_dev->block_ops->release))
        return;

    block_dev->block_ops->release(block_dev->internal_id);
}

bool dev_block_is_proxy(int dev_id) {
    dev_t* dev = dev_block_get(dev_id);
    if (dev == NULL)
        return false;

    dev_block_t* block_dev = dev->type_specific;
    return block_dev->proxy_to != NULL;
}

dev_block_t* dev_block_get_data(int dev_id) {
    dev_t* dev = dev_block_get(dev_id);
    if (dev == NULL)
        return NULL;

    dev_block_t* block_dev = dev->type_specific;

    if (block_dev->proxy_to != NULL)
        block_dev = block_dev->proxy_to;

    return block_dev;
}
