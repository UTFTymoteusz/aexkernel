#include "aex/mem.h"
#include "aex/rcode.h"
#include "aex/rcparray.h"

#include "aex/dev/dev.h"
#include "aex/dev/char.h"

extern rcparray(dev_t*) dev_array;

int dev_register_char(char* name, struct dev_char* dev_char) {
    struct dev* dev = (struct dev*) kmalloc(sizeof(struct dev));

    dev->type = DEV_TYPE_CHAR;
    dev->type_specific = dev_char;
    snprintf(dev->name, sizeof(dev->name), "%s", name);

    dev_char->access.val  = 0;
    dev_char->access.name = NULL;
    
    dev_char->worker = -1;

    int ret = dev_register(dev);
    IF_ERROR(ret)
        kfree((void*) dev);
        
    return ret;
}

void dev_unregister_char(char* name) {
    name = name;
    kpanic("dev_unregister_char() is unimplemented");
}

dev_char_t* dev_char_get(int dev_id) {
    dev_t** _dev = rcparray_get(dev_array, dev_id);
    if (_dev == NULL)
        return NULL;

    dev_t* dev = *_dev;

    if (dev->type != DEV_TYPE_CHAR) {
        rcparray_unref(dev_array, dev_id);
        return NULL;
    }
    return dev->type_specific;
}

void dev_char_unref(int dev_id) {
    rcparray_unref(dev_array, dev_id);
}

int dev_char_open(int id, dev_fd_t* dfd) {
    dev_char_t* dev_char = dev_char_get(id);
    if (dev_char == NULL)
        return -ENODEV;

    int ret = dev_char->char_ops->open(dev_char->internal_id, dfd);
    dev_char_unref(id);

    return ret;
}

int dev_char_read(int id, dev_fd_t* dfd, uint8_t* buffer, int len) {
    dev_char_t* dev_char = dev_char_get(id);
    if (dev_char == NULL)
        return -ENODEV;

    if (dev_char->char_ops->read == NULL) {
        dev_char_unref(id);
        return -ENOSYS;
    }
    int ret = dev_char->char_ops->read(dev_char->internal_id, dfd, buffer, len);
    dev_char_unref(id);

    return ret;
}

int dev_char_write(int id, dev_fd_t* dfd, uint8_t* buffer, int len) {
    dev_char_t* dev_char = dev_char_get(id);
    if (dev_char == NULL)
        return -ENODEV;

    if (dev_char->char_ops->write == NULL) {
        dev_char_unref(id);
        return -ENOSYS;
    }
    int ret = dev_char->char_ops->write(dev_char->internal_id, dfd, buffer, len);
    dev_char_unref(id);

    return ret;
}

int dev_char_close(int id, dev_fd_t* dfd) {
    dev_char_t* dev_char = dev_char_get(id);
    if (dev_char == NULL)
        return -ENODEV;

    dev_char->char_ops->close(dev_char->internal_id, dfd);
    dev_char_unref(id);

    return 0;
}

long dev_char_ioctl(int id, dev_fd_t* dfd, long code, void* mem) {
    dev_char_t* dev_char = dev_char_get(id);
    if (dev_char == NULL)
        return -ENODEV;

    if (dev_char->char_ops->ioctl == NULL){
        dev_char_unref(id);
        return -ENOSYS;
    }
    long ret = dev_char->char_ops->ioctl(dev_char->internal_id, dfd, code, mem);
    dev_char_unref(id);

    return ret;
}