#include "aex/io.h"
#include "aex/klist.h"
#include "aex/math.h"
#include "aex/mem.h"
#include "aex/rcode.h"
#include "aex/string.h"
#include "aex/sys.h"

#include "aex/proc/exec.h"
#include "aex/proc/proc.h"

#include "aex/dev/dev.h"
#include "aex/dev/block.h"

#define MAX_BYTES_PER_RQ 65536

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
        spinlock_acquire(&(blk->access));
        brq = blk->io_queue;

        if (brq == NULL) {
            spinlock_release(&(blk->access));
            io_sblock();
            continue;
        }
        spinlock_release(&(blk->access));

        requester_pid = brq->thread->parent_pid;
        if (!process_use(requester_pid)) {
            brq->response = ERR_INTERRUPTED;
            brq->done = true;

            spinlock_acquire(&(blk->access));
            blk->io_queue = brq->next;
            spinlock_release(&(blk->access));

            continue;
        }
        kp_change_dir(process_get(requester_pid)->proot);
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
        spinlock_acquire(&(blk->access));
        brq->done = true;
        process_release(requester_pid);

        blk->io_queue = brq->next;
        spinlock_release(&(blk->access));
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

        snprintf(th->name, strlen(dev->name) + 20, "Block dev '%s' worker", dev->name);
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
    spinlock_acquire(&(block_dev->access));

    if (block_dev->io_queue == NULL)
        block_dev->io_queue = &brq;
    else
        block_dev->last_brq->next = &brq;
        
    block_dev->last_brq = &brq;
    
    spinlock_release(&(block_dev->access));

    io_sunblock(block_dev->worker->task);
    while (!(brq.done))
        yield();

    return (int) brq.response;
}

int queue_io_wait(dev_block_t* block_dev, uint64_t sector, uint16_t count, uint8_t* buffer, bool write) {
    blk_request_t brq = {
        .type   = write ? BLK_WRITE : BLK_READ,
        .sector = sector,
        .count  = count,
        .buffer = buffer,

        .thread = task_current->thread,

        .done = false,
        .next = NULL,
    };
    spinlock_acquire(&(block_dev->access));
    
    if (block_dev->io_queue == NULL)
        block_dev->io_queue = &brq;
    else
        block_dev->last_brq->next = &brq;
        
    block_dev->last_brq = &brq;

    spinlock_release(&(block_dev->access));

    io_sunblock(block_dev->worker->task);
    while (!(brq.done))
        yield();

    return (int) brq.response;
}

int dev_block_read(int dev_id, uint8_t* buffer, uint64_t pos, uint64_t len) {
    if (len == 0)
        return 0;

    dev_t* dev = dev_block_get(dev_id);
    if (dev == NULL)
        return ERR_NOT_FOUND;

    dev_block_t* block_dev = dev->type_specific;

    if (block_dev->proxy_to != NULL) {
        pos += (block_dev->proxy_offset * 512);
        block_dev = block_dev->proxy_to;
    }
    if (!block_dev->initialized)
        dev_block_init(dev_id);

    if (!(block_dev->block_ops->read))
        return ERR_NOT_IMPLEMENTED;

    uint32_t sector_size  = block_dev->sector_size;

    uint32_t read_offset    = pos % sector_size;
    uint32_t current_sector = pos / sector_size;
    uint32_t sector_count = 
        (read_offset + len + (sector_size - 1)) / sector_size;

    uint32_t buffer_sector_count = sector_count;
    
    buffer_sector_count = min(buffer_sector_count, block_dev->burst_max);
    buffer_sector_count = min(buffer_sector_count, MAX_BYTES_PER_RQ / sector_size);

    CLEANUP uint8_t* bouncer = kmalloc(buffer_sector_count * sector_size);

    // make it not-copy when possible
    while (sector_count > 0) {
        uint16_t burst_count = sector_count;

        burst_count = min(burst_count, block_dev->burst_max);
        burst_count = min(burst_count, MAX_BYTES_PER_RQ / sector_size);

        int ret = queue_io_wait(block_dev, 
                    current_sector, burst_count, bouncer, false);
        RETURN_IF_ERROR(ret);

        uint32_t copy_len = (burst_count * sector_size) - read_offset;
        copy_len = min(copy_len, len);
        
        memcpy(buffer, bouncer + read_offset, copy_len);
        buffer += copy_len;

        read_offset = 0;

        current_sector += burst_count;
        sector_count   -= burst_count;
    }
    return 0;
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
    spinlock_acquire(&(block_dev->access));
    if (block_dev->io_queue == NULL)
        block_dev->io_queue = &brq;
    else
        block_dev->last_brq->next = &brq;
        
    block_dev->last_brq = &brq;
    spinlock_release(&(block_dev->access));

    io_sunblock(block_dev->worker->task);
    while (!(brq.done))
        yield();

    spinlock_wait(&(block_dev->access));
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
