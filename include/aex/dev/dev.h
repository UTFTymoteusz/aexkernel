#pragma once

#include "aex/klist.h"
#include "aex/fs/fd.h"
#include "aex/fs/fs.h"

enum dev_type {
    DEV_TYPE_CHAR  = 1,
    DEV_TYPE_BLOCK = 2,
    DEV_TYPE_NET   = 3,
};

struct dev_fd {
    void* data;
};
typedef struct dev_fd dev_fd_t;

struct dev_char_ops {
    int  (*open) (int fd, dev_fd_t* dfd);
    int  (*read) (int fd, dev_fd_t* dfd, uint8_t* buffer, int len);
    int  (*write)(int fd, dev_fd_t* dfd, uint8_t* buffer, int len);
    void (*close)(int fd, dev_fd_t* dfd);
    long (*ioctl)(int fd, dev_fd_t* dfd, long, void*);
};

struct dev {
    char name[32];

    uint8_t type;
    void*   type_specific;
};
typedef struct dev dev_t;

int dev_register(dev_t* dev);

int dev_current_amount();
int dev_list(dev_t** list);

bool dev_exists(int id);

/*
 * Returns the type of a device.
 */
int dev_type(int id);