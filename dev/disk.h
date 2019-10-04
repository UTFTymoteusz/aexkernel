#pragma once

#include "disk.c"

enum disk_flags;

struct dev_disk;
struct dev_disk_ops;

typedef struct dev_disk dev_disk_t;

int dev_register_disk(char* name, struct dev_disk* disk);
void dev_unregister_disk(char* name);

int dev_disk_init(int dev_id);
int dev_disk_read(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer);
int dev_disk_write(int dev_id, uint64_t sector, uint16_t count, uint8_t* buffer);
void dev_disk_release(int dev_id);

dev_disk_t* dev_disk_get_data(int dev_id);