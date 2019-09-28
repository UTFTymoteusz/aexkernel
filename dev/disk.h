#pragma once

#include "dev/disk.c"

struct dev_disk_request;
typedef struct dev_disk_request dev_disk_request_t;

struct dev_disk;
struct dev_disk_ops;

int dev_register_disk(char* name, struct dev_disk* disk);
void dev_unregister_disk(char* name);