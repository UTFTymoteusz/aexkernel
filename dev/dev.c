#include "aex/mem.h"
#include "aex/rcode.h"
#include "aex/klist.h"
#include "aex/rcparray.h"
#include "aex/string.h"

#include "aex/dev/name.h"

#include "aex/fs/fd.h"

#include "kernel/init.h"
#include "aex/dev/dev.h"

rcparray(dev_t*) dev_array;

extern struct klist dev_incrementations;

int dev_register(dev_t* dev) {
    int id = rcparray_add(dev_array, dev);
    return id;
}

int dev_current_amount() {
    int amnt = 0;
    int id   = -1;

    rcparray_foreach(dev_array, id)
        amnt++;

    return amnt;
}

bool dev_exists(int id) {
    dev_t** _dev = rcparray_get(dev_array, id);
    if (_dev != NULL)
        rcparray_unref(dev_array, id);

    return _dev != NULL;
}

int dev_type(int id) {
    dev_t** _dev = rcparray_get(dev_array, id);
    if (_dev == NULL)
        return -1;

    int ret = (*_dev)->type;
    rcparray_unref(dev_array, id);

    return ret;
}

void class_disk_init();

void dev_init() {
    class_disk_init();
    klist_init(&dev_incrementations);
}