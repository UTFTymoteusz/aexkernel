#include "aex/aex.h"
#include "aex/dev/char.h"
#include "aex/dev/dev.h"

#include <stdint.h>

#include "null.h"

int  pseudo_null_open (int fd, dev_fd_t* file);
int  pseudo_null_read (int fd, dev_fd_t* file, uint8_t* buffer, int len);
int  pseudo_null_write(int fd, dev_fd_t* file, uint8_t* buffer, int len);
void pseudo_null_close(int fd, dev_fd_t* file);

struct dev_char_ops pseudo_null_ops = {
    .open  = pseudo_null_open,
    .read  = pseudo_null_read,
    .write = pseudo_null_write,
    .close = pseudo_null_close,
};
struct dev_char pseudo_null_dev = {
    .char_ops    = &pseudo_null_ops,
    .internal_id = 0,
};

void pseudo_null_init() {
    dev_register_char("null", &pseudo_null_dev);
}

int pseudo_null_open(UNUSED int fd, UNUSED dev_fd_t* file) {
    return 0;
}
int pseudo_null_read(UNUSED int fd, UNUSED dev_fd_t* file, UNUSED uint8_t* buffer, UNUSED int len) {
    return -1;
}
int pseudo_null_write(UNUSED int fd, UNUSED dev_fd_t* file, UNUSED uint8_t* buffer, int len) {
    return len;
}
void pseudo_null_close(UNUSED int fd, UNUSED dev_fd_t* file) {
    return;
}