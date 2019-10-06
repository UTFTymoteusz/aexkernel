#pragma once

#include "dev.c"

#include "aex/klist.h"

enum dev_type;

struct dev_file_ops;
struct dev;
struct dev* dev_array[DEV_ARRAY_SIZE];

typedef struct dev dev_t;

struct klist disk_devs;

void dev_init();
int  dev_register(dev_t* dev);

int dev_current_amount();
int dev_list(dev_t** list);

bool dev_exists(int id);

int dev_open(int id);
int dev_write(int id, uint8_t* buffer, int len);
int dev_close(int id);