#pragma once

#include "dev/dev.h"

struct dev_disk_request {
    int rq;
    uint64_t sector;
    char* buffer;
    bool write;
};
typedef struct dev_disk_request dev_disk_request_t;

struct dev_disk {
    uint32_t total_sectors;
    uint32_t sector_size;

    struct dev_file_ops* ops;
    struct dev_disk_ops* disk_ops;
};
struct dev_disk_ops {
    int (*disk_request)(struct dev_disk_request*);
};

int dev_register_disk(char* name, struct dev_disk* disk) {
    kpanic("dev_register_disk() is unimplemented");
    return -1;
}
void dev_unregister_disk(char* name) {
    kpanic("dev_unregister_disk() is unimplemented");
}