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
typedef struct dev_incr_entry {
    char* pattern;
    char  c;
} dev_incr_t;
struct dev* dev_list[DEV_COUNT];

struct klist dev_incrementations;

void dev_init() {
    klist_init(&dev_incrementations);
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

// This accepts an input like "sd@" and spits out sda, sdb, sdc and so on
char* dev_increment_name(char* pattern, char* buffer) {
    
    klist_entry_t* klist_entry = NULL;
    dev_incr_t* entry = NULL;

    while (true) {
        entry = (dev_incr_t*)klist_iter(&dev_incrementations, &klist_entry);

        if (entry == NULL)
            break;

        if (!strcmp(entry->pattern, pattern))
            break;
    }

    if (entry == NULL) {
        entry = kmalloc(sizeof(dev_incr_t));
        klist_set(&dev_incrementations, dev_incrementations.count + 1, (void*)entry);

        entry->pattern = kmalloc(8);
        entry->c = 'a';
        memcpy(entry->pattern, pattern, 8);
    }
    else
        entry->c++;

    int i = 0;
    while (pattern[i] != '\0') {
        
        switch (pattern[i]) { 
            case '@':
                buffer[i] = entry->c;
                break;
            default:
                buffer[i] = pattern[i];
                break;
        }
        ++i;
    }
    return buffer;
}