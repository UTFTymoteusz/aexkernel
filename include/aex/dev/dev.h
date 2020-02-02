#pragma once

#include "aex/klist.h"
#include "aex/fs/file.h"

#define DEV_ARRAY_SIZE 256

enum dev_type {
    DEV_TYPE_CHAR  = 1,
    DEV_TYPE_BLOCK = 2,
    DEV_TYPE_NET   = 3,
};

struct dev_file_ops {
    int  (*open) (int fd, file_t* file);
    int  (*read) (int fd, file_t* file, uint8_t* buffer, int len);
    int  (*write)(int fd, file_t* file, uint8_t* buffer, int len);
    void (*close)(int fd, file_t* file);
    long (*ioctl)(int fd, file_t* file, long, void*);
};
struct dev {
    uint8_t type;

    char name[32];
    void* type_specific;

    struct dev_file_ops* ops;
};
typedef struct dev dev_t;

struct dev* dev_array[DEV_ARRAY_SIZE];

typedef struct dev dev_t;

int dev_register(char* name, dev_t* dev);

int dev_current_amount();
int dev_list(dev_t** list);

bool dev_exists(int id);