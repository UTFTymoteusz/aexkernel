#include "aex/kmem.h"
#include "kernel/sys.h"
#include "dev.h"

#include "char.h"

int dev_register_char(char* name, struct dev_char* dev_char) {
    struct dev* dev = (struct dev*)kmalloc(sizeof(struct dev));

    dev->type = DEV_TYPE_CHAR;
    dev->name = name;
    dev->ops  = dev_char->ops;
    dev->type_specific = dev_char;

    int ret = dev_register(dev);
    if (ret < 0)
        kfree((void*)dev);

    return ret;
}

void dev_unregister_char(char* name) {
    name = name;
    kpanic("dev_unregister_char() is unimplemented");
}