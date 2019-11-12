#include "aex/kmem.h"
#include "aex/rcode.h"

#include "kernel/sys.h"

#include "dev.h"
#include "char.h"

void chr_worker(dev_t* dev) {
    chr_request_t* crq;
    process_t* process;

    dev_char_t* chr = dev->type_specific;

    while (true) {
        mutex_acquire_yield(&(chr->access));
        crq = chr->io_queue;
        mutex_release(&(chr->access));

        if (crq == NULL) {
            io_sblock();
            continue;
        }

        process = crq->thread->process;
        process_use(process);

        switch (crq->type) {
            case CHR_OPEN:
                break;
            case CHR_READ:
                break;
            case CHR_WRITE:
                break;
            case CHR_CLOSE:
                break;
            default:
                crq->response = 0;
                break;
        }
        crq->done = true;

        io_sunblock(crq->thread->task);
        process_release(process);

        mutex_acquire_yield(&(chr->access));
        chr->io_queue = crq->next;
        mutex_release(&(chr->access));
    }
}

int dev_register_char(char* name, struct dev_char* dev_char) {
    struct dev* dev = (struct dev*)kmalloc(sizeof(struct dev));

    dev->type = DEV_TYPE_CHAR;
    dev->name = name;
    dev->ops  = dev_char->ops;
    dev->type_specific = dev_char;

    int ret = dev_register(dev);
    if (ret < 0) {
        kfree((void*) dev);
        return ret;
    }

    return ret;
}

void dev_unregister_char(char* name) {
    name = name;
    kpanic("dev_unregister_char() is unimplemented");
}

int dev_char_open(int id) {
    if (dev_array[id] == NULL)
        return DEV_ERR_NOT_FOUND;

    struct dev_char* dev_char = (struct dev_char*)dev_array[id]->type_specific;
    return dev_array[id]->ops->open(dev_char->internal_id);
}

int dev_char_read(int id, uint8_t* buffer, int len) {
    struct dev_char* dev_char = (struct dev_char*)dev_array[id]->type_specific;
    return dev_array[id]->ops->read(dev_char->internal_id, buffer, len);
}

int dev_char_write(int id, uint8_t* buffer, int len) {
    struct dev_char* dev_char = (struct dev_char*)dev_array[id]->type_specific;
    return dev_array[id]->ops->write(dev_char->internal_id, buffer, len);
}

int dev_char_close(int id) {
    if (dev_array[id] == NULL)
        return DEV_ERR_NOT_FOUND;

    struct dev_char* dev_char = (struct dev_char*)dev_array[id]->type_specific;
    dev_array[id]->ops->close(dev_char->internal_id);
    return 0;
}

long dev_char_ioctl(int id, long code, void* mem) {
    if (dev_array[id] == NULL)
        return DEV_ERR_NOT_FOUND;

    struct dev_char* dev_char = (struct dev_char*)dev_array[id]->type_specific;
    return dev_array[id]->ops->ioctl(dev_char->internal_id, code, mem);
}