#pragma once

#include "aex/klist.h"

#define DEV_COUNT 64

enum dev_type {
    DEV_TYPE_CHAR = 1,
    DEV_TYPE_DISK = 2,
    DEV_TYPE_NET = 3,
};

struct dev_file_ops {
    int (*open)();
    int (*read)(char* buffer, int len);
    int (*write)(char* buffer, int len);
    void (*close)();
    long (*ioctl)(long, long);
};
struct dev {
    uint8_t type;
    char* name;
    void* data;
    struct dev_file_ops* ops;
};
struct dev* dev_list[DEV_COUNT];

void dev_init() {
    
}
int dev_register(struct dev* dev) {

    for (size_t i = 0; i < DEV_COUNT; i++) {

        if (dev_list[i] == NULL) {
            dev_list[i] = dev;
            return i;
        }
    }
    return -1;
}

int dev_open(int id) {
    
    if (dev_list[id] == NULL)
        return -1;

    return dev_list[id]->ops->open();
}

int dev_write(int id, char* buffer, int len) {
    return dev_list[id]->ops->write(buffer, len);
}

int dev_name_to_id(char* name) {

    for (size_t i = 0; i < DEV_COUNT; i++) {

        if (dev_list[i] == NULL)
            continue;

        if (dev_list[i]->name == name)
            return i;
    }
    return -1;
}