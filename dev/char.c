#include "aex/mem.h"
#include "aex/rcode.h"
#include "aex/sys.h"

#include "aex/dev/dev.h"
#include "aex/dev/char.h"

int dev_register_char(char* name, struct dev_char* dev_char) {
    struct dev* dev = (struct dev*) kmalloc(sizeof(struct dev));

    dev->type = DEV_TYPE_CHAR;
    dev->ops  = dev_char->ops;
    dev->type_specific = dev_char;

    dev_char->access.val  = 0;
    dev_char->access.name = NULL;
    
    dev_char->worker = NULL;

    int ret = dev_register(name, dev);
    IF_ERROR(ret)
        kfree((void*) dev);
        
    return ret;
}

void dev_unregister_char(char* name) {
    name = name;
    kpanic("dev_unregister_char() is unimplemented");
}

int dev_char_open(int id, dev_fd_t* dfd) {
    if (dev_array[id] == NULL)
        return ERR_NOT_FOUND;

    struct dev_char* dev_char = (struct dev_char*) dev_array[id]->type_specific;

    return dev_array[id]->ops->open(dev_char->internal_id, dfd);
}

int dev_char_read(int id, dev_fd_t* dfd, uint8_t* buffer, int len) {
    struct dev_char* dev_char = (struct dev_char*) dev_array[id]->type_specific;

    if (dev_array[id]->ops->read == NULL)
        return ERR_NOT_IMPLEMENTED;

    return dev_array[id]->ops->read(dev_char->internal_id, dfd, buffer, len);
}

int dev_char_write(int id, dev_fd_t* dfd, uint8_t* buffer, int len) {
    struct dev_char* dev_char = (struct dev_char*) dev_array[id]->type_specific;

    if (dev_array[id]->ops->write == NULL)
        return ERR_NOT_IMPLEMENTED;

    return dev_array[id]->ops->write(dev_char->internal_id, dfd, buffer, len);
}

int dev_char_close(int id, dev_fd_t* dfd) {
    if (dev_array[id] == NULL)
        return ERR_NOT_FOUND;

    struct dev_char* dev_char = (struct dev_char*) dev_array[id]->type_specific;
    
    dev_array[id]->ops->close(dev_char->internal_id, dfd);
    return 0;
}

long dev_char_ioctl(int id, dev_fd_t* dfd, long code, void* mem) {
    if (dev_array[id] == NULL)
        return ERR_NOT_FOUND;

    struct dev_char* dev_char = (struct dev_char*) dev_array[id]->type_specific;

    if (dev_array[id]->ops->ioctl == NULL)
        return ERR_NOT_IMPLEMENTED;

    return dev_array[id]->ops->ioctl(dev_char->internal_id, dfd, code, mem);
}