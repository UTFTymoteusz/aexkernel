#include "aex/rcode.h"
#include "aex/klist.h"

#include "dev/name.h"

#include "dev.h"

enum dev_type;

struct dev_file_ops;
struct dev;

typedef struct dev dev_t;

dev_t* dev_array[DEV_ARRAY_SIZE];

extern struct klist dev_incrementations;

void dev_init() {
    klist_init(&dev_incrementations);
}

int dev_register(dev_t* dev) {
    for (size_t i = 0; i < DEV_ARRAY_SIZE; i++) {
        if (dev_array[i] == NULL) {
            dev_array[i] = dev;
            return i;
        }
    }
    return ERR_NO_SPACE;
}

int dev_current_amount() {
    int amnt = 0;

    for (size_t i = 0; i < DEV_ARRAY_SIZE; i++)
        if (dev_array[i] != NULL)
            amnt++;

    return amnt;
}

int dev_list(dev_t** list) {
    int list_ptr = 0;

    for (size_t i = 0; i < DEV_ARRAY_SIZE; i++) {
        if (dev_array[i] == NULL)
            continue;

        list[list_ptr++] = dev_array[i];
    }
    return list_ptr;
}

bool dev_exists(int id) {
    return !(dev_array[id] == NULL);
}

int dev_open(int id) {
    if (dev_array[id] == NULL)
        return DEV_ERR_NOT_FOUND;

    return dev_array[id]->ops->open();
}

int dev_write(int id, uint8_t* buffer, int len) {
    return dev_array[id]->ops->write(buffer, len);
}

int dev_close(int id) {
    if (dev_array[id] == NULL)
        return DEV_ERR_NOT_FOUND;

    dev_array[id]->ops->close();
    return 0;
}