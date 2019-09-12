#pragma once

#include "dev/dev.c"

#include "aex/klist.h"

enum dev_type;

struct dev;
struct dev* dev_list[DEV_COUNT];

struct klist disk_devs;

void dev_init();
int dev_register();

int dev_name_to_id(char* name);