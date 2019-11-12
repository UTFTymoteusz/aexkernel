#pragma once

#include "aex/klist.h"

#define DEV_ARRAY_SIZE 256

enum dev_type {
    DEV_TYPE_CHAR = 1,
    DEV_TYPE_BLOCK = 2,
    DEV_TYPE_NET  = 3,
};

struct dev_file_ops {
    int  (*open)(int fd);
    int  (*read)(int fd, uint8_t* buffer, int len);
    int  (*write)(int fd, uint8_t* buffer, int len);
    void (*close)(int fd);
    long (*ioctl)(int fd, long, void*);
};
struct dev {
    uint8_t type;

    char* name;
    void* type_specific;

    struct dev_file_ops* ops;
};
typedef struct dev dev_t;

struct dev* dev_array[DEV_ARRAY_SIZE];

typedef struct dev dev_t;

void dev_init();
int  dev_register(dev_t* dev);

int dev_current_amount();
int dev_list(dev_t** list);

bool dev_exists(int id);