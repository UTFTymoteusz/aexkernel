#include "aex/io.h"
#include "aex/mem.h"
#include "aex/klist.h"
#include "aex/rcode.h"
#include "aex/string.h"
#include "aex/sys.h"

#include "aex/proc/exec.h"
#include "aex/proc/proc.h"

#include "mem/page.h"

#include "aex/dev/dev.h"
#include "aex/dev/block.h"

enum block_flags;

struct dev_block;
struct dev_block_ops;

typedef struct dev_block dev_block_t;

struct klist block_devs;

void blk_worker(dev_t* dev) {
    blk_request_t* brq;
    pid_t requester_pid;

    dev_block_t* blk = dev->type_specific;

    while (true) {
        mutex_acquire_yield(&(blk->access));
        brq = blk->io_queue;

        if (brq == NULL) {
            mutex_release(&(blk->access));
            io_sblock();
            continue;
        }
        mutex_release(&(blk->access));

        requester_pid = brq->thread->parent_pid;
        if (!process_use(requester_pid)) {
            brq->response = ERR_TERMINATING;
            brq->done = true;

            mutex_acquire_yield(&(blk->access));
            blk->io_queue = brq->next;
            mutex_release(&(blk->access));

            continue;
        }
        mempg_set_pagedir(process_get(requester_pid)->proot);

        switch (brq->type) {
            case BLK_INIT:
                if (blk->initialized) {
                    brq->response = ERR_ALREADY_DONE;
                    break;
                }
                if (!(blk->block_ops->init)){
                    brq->response = ERR_NOT_IMPLEMENTED;
                    break;
                }
                brq->response = blk->block_ops->init(blk->internal_id);
                if (brq->response >= 0)
                    blk->initialized = true;

                break;
            case BLK_READ:
                brq->response = blk->block_ops->read(blk->internal_id, brq->sector, brq->count, brq->buffer);
                break;
            case BLK_WRITE:
                brq->response = blk->block_ops->write(blk->internal_id, brq->sector, brq->count, brq->buffer);
                break;
            case BLK_RELEASE:
                if (!(blk->initialized)) {
                    brq->response = ERR_ALREADY_DONE;
                    break;
                }
                if (!(blk->block_ops->release)){
                    brq->response = ERR_NOT_IMPLEMENTED;
                    break;
                }
                brq->response = blk->block_ops->release(blk->internal_id);
                if (brq->response >= 0)
                    blk->initialized = false;
                
                break;
            default:
                brq->response = 0;
                break;
        }
        mutex_acquire_yield(&(blk->access));
        brq->done = true;
        process_release(requester_pid);

        blk->io_queue = brq->next;
        mutex_release(&(blk->access));
    }
}

int dev_register_block(char* name, struct dev_block* block_dev) {
    dev_t* dev = kmalloc(sizeof(dev_t));
    dev->type = DEV_TYPE_BLOCK;
    dev->type_specific = block_dev;

    block_dev->access.val  = 0;
    block_dev->access.name = "block device access";

    int ret = dev_register(name, dev);
    IF_ERROR(ret) {
        kfree(dev);
        return ret;
    }
    tid_t th_id = thread_create(KERNEL_PROCESS, blk_worker, true);
    if (process_lock(KERNEL_PROCESS)) {
        thread_t* th = thread_get(KERNEL_PROCESS, th_id);

        set_arguments(th->task, 1, dev);
        th->name = kmalloc(strlen(dev->name) + 20);

        sprintf(th->name, "Block dev '%s' worker", dev->name);
        block_dev->worker = th;

        process_unlock(KERNEL_PROCESS);
    }
    thread_start(KERNEL_PROCESS, th_id);
    
    return ret;
}

void dev_unregister_block(char* name) {
    name = name; // temp
    kpanic("dev_unregister_block() is unimplemented");
}

static inline dev_t* dev_block_get(int dev_id) {
    dev_t* dev = dev_array[dev_id];

    if (dev == NULL || dev->type != DEV_TYPE_BLOCK)
        return NULL;

    return dev;
}

int dev_block_init(int dev_id) {
    dev_t* dev = dev_block_get(dev_id);
    if (dev == NULL)
        return ERR_NOT_FOUND;

    dev_block_t* block_dev = dev->type_specific;

    if (block_dev->proxy_to != NULL)
        block_dev = block_dev->proxy_to;

    if (block_dev->initialized)
        return ERR_ALREADY_DONE;

    blk_request_t brq = {
        .type = BLK_INIT,
        .thread = task_current->thread,

        .done = false,
        .next = NULL,
    };
    mutex_acquire_yield(&(block_dev->access));

    if (block_dev->io_queue == NULL)
        block_dev->io_queue = &brq;
    else
        block_dev->last_brq->next = &brq;
        
    block_dev->last_brq = &brq;
    
    mutex_release(&(block_dev->access));

    io_sunblock(block_dev->worker->task);
    while (!(brq.done))
        yield();

    return (int) brq.response;
}

int queue_io_wait(dev_block_t* block_dev, uint64_t sector, uint16_t count, uint8_t* buffer, bool write) {
    blk_request_t brq = {
        .type = write ? BLK_WRITE : BLK_READ,
        .sector = sector,
        .count  = count,
        .buffer = buffer,

        .thread = task_current->thread,

        .done = false,
        .next = NULL,
    };
    mutex_acquire_yield(&(block_dev->access));
    
    if (block_dev->io_queue == NULL)
        block_dev->io_queue = &brq;
    else
        block_dev->last_brq->next = &brq;
        
    block_dev->last_brq = &brq;

    mutex_release(&(block_dev->access));

    io_sunblock(block_dev->worker->task);
    while (!(brq.done))
        yield();

    return (int) brq.response;
}

// Make this utilize the IO queue better
int dev_block_read(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer) {
    dev_t* dev = dev_block_get(dev_id);
    if (dev == NULL)
        return ERR_NOT_FOUND;

    dev_block_t* block_dev = dev->type_specific;

    if (block_dev->proxy_to != NULL) {
        sector += block_dev->proxy_offset;
        block_dev = block_dev->proxy_to;
    }
    if (!block_dev->initialized)
        dev_block_init(dev_id);

    if (!(block_dev->block_ops->read))
        return ERR_NOT_IMPLEMENTED;

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
            CLEANUP void* bounce_buffer = kmalloc(sector_size);

            ret = queue_io_wait(block_dev, sector, 1, bounce_buffer, false);
            RETURN_IF_ERROR(ret);

            bytes_remaining -= (sector_size - offset);
            memcpy(buffer, (void*) (((size_t) bounce_buffer) + offset), sector_size - offset);

            buffer += sector_size - offset;
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
            ret = queue_io_wait(block_dev, sector, (count2 > msat) ? msat : count2, buffer, false);
            RETURN_IF_ERROR(ret);

            buffer2 += block_dev->sector_size * msat;
            sector  += msat;
            count2  -= msat;
        }
        if (bytes_remaining == 0 || ret < 0)
            return ret;

        buffer += partial_count * sector_size;

        CLEANUP void* bounce_buffer = kmalloc(sector_size);

        ret = queue_io_wait(block_dev, sector, 1, bounce_buffer, false);
        memcpy(buffer, bounce_buffer, bytes_remaining);
        
        return ret;
    }
    return block_dev->block_ops->read(block_dev->internal_id, sector, count, buffer);
}

int dev_block_dread(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer) {
    dev_t* dev = dev_block_get(dev_id);
    if (dev == NULL)
        return ERR_NOT_FOUND;

    dev_block_t* block_dev = dev->type_specific;

    if (block_dev->proxy_to != NULL) {
        sector += block_dev->proxy_offset;
        block_dev = block_dev->proxy_to;
    }
    if (!block_dev->initialized)
        dev_block_init(dev_id);

    if (!(block_dev->block_ops->read))
        return ERR_NOT_IMPLEMENTED;

    int32_t count2 = count;
    int16_t msat = block_dev->max_sectors_at_once;

    int ret;

    while (count2 > 0) {
        ret = queue_io_wait(block_dev, sector, (count2 > msat) ? msat : count2, buffer, false);
        RETURN_IF_ERROR(ret);

        buffer += block_dev->sector_size * msat;
        count2 -= msat;
        sector += msat;
    }
    return 0;
}

int dev_block_write(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer) {
    dev_t* dev = dev_block_get(dev_id);
    if (dev == NULL)
        return ERR_NOT_FOUND;

    dev_block_t* block_dev = dev->type_specific;

    if (block_dev->proxy_to != NULL) {
        sector += block_dev->proxy_offset;
        block_dev = block_dev->proxy_to;
    }
    if (!block_dev->initialized)
        dev_block_init(dev_id);

    if (!(block_dev->block_ops->write))
        return ERR_NOT_IMPLEMENTED;

    return queue_io_wait(block_dev, sector, count, buffer, true);
}

int dev_block_dwrite(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer) {
    dev_t* dev = dev_block_get(dev_id);
    if (dev == NULL)
        return ERR_NOT_FOUND;

    dev_block_t* block_dev = dev->type_specific;

    if (block_dev->proxy_to != NULL) {
        sector += block_dev->proxy_offset;
        block_dev = block_dev->proxy_to;
    }
    if (!block_dev->initialized)
        dev_block_init(dev_id);

    if (!(block_dev->block_ops->write))
        return ERR_NOT_IMPLEMENTED;

    return queue_io_wait(block_dev, sector, count, buffer, true);
}

int dev_block_release(int dev_id) {
    dev_t* dev = dev_block_get(dev_id);
    if (dev == NULL)
        return ERR_NOT_FOUND;

    dev_block_t* block_dev = dev->type_specific;

    if (block_dev->proxy_to != NULL)
        block_dev = block_dev->proxy_to;

    if (!(block_dev->initialized))
        return ERR_ALREADY_DONE;

    blk_request_t brq = {
        .type = BLK_RELEASE,
        .thread = task_current->thread,
        
        .done = false,
        .next = NULL,
    };
    mutex_acquire_yield(&(block_dev->access));
    if (block_dev->io_queue == NULL)
        block_dev->io_queue = &brq;
    else
        block_dev->last_brq->next = &brq;
        
    block_dev->last_brq = &brq;
    mutex_release(&(block_dev->access));

    io_sunblock(block_dev->worker->task);
    while (!(brq.done))
        yield();

    mutex_wait_yield(&(block_dev->access));
    return (int) brq.response;
}

int dev_block_set_proxy(struct dev_block* block_dev, struct dev_block* proxy_to) {
    block_dev->proxy_to = proxy_to;
    if (block_dev->worker != NULL) {
        //thread_kill(block_dev->worker);
        block_dev->worker = NULL;
    }
    return 0;
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
