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
int  dev_register();