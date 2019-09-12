#pragma once

#include "dev/dev.h"

struct dev_disk {
    struct dev_disk_ops* ops;
};
struct dev_disk_ops {
    int (*open)();
    int (*close)();
};

int dev_register_disk(char* name, struct dev_disk* disk) {
    kpanic("dev_register_disk() is unimplemented");
}
void dev_unregister_disk(char* name) {
    kpanic("dev_unregister_disk() is unimplemented");
}