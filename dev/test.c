#include "aex/dev/char.h"
#include "aex/dev/dev.h"

#include "aex/rcode.h"
#include "aex/mem.h"

#include "dev/test.h"

int  test_open (int fd, dev_fd_t* file);
int  test_read (int fd, dev_fd_t* file, uint8_t* buffer, int len);
int  test_write(int fd, dev_fd_t* file, uint8_t* buffer, int len);
void test_close(int fd, dev_fd_t* file);
long test_ioctl(int fd, dev_fd_t* file, long code, void* mem);

struct dev_char_ops test_ops = {
    .open  = test_open,
    .read  = test_read,
    .write = test_write,
    .close = test_close,
    .ioctl = test_ioctl,
};

int test_open(int fd, UNUSED dev_fd_t* file) {
    if (fd != 66)
        kpanic("testdev test failed: open(): Internal ID mismatch");

    printk("testdev: Opened\n");
    return 0;
}

int test_read(int fd, UNUSED dev_fd_t* file, UNUSED uint8_t* buffer, UNUSED int len) {
    if (fd != 66)
        kpanic("testdev test failed: read(): Internal ID mismatch");

    printk("testdev: Read from\n");
    return -ENOSYS;
}

int test_write(int fd, UNUSED dev_fd_t* file, UNUSED uint8_t* buffer, int len) {
    if (fd != 66)
        kpanic("testdev test failed: write(): Internal ID mismatch");

    printk("testdev: Written to\n");
    return len;
}

void test_close(int fd, UNUSED dev_fd_t* file) {
    if (fd != 66)
        kpanic("testdev test failed: open(): Internal ID mismatch");

    printk("testdev: Closed\n");
}

long test_ioctl(int fd, UNUSED dev_fd_t* file, UNUSED long code, void* mem) {
    static int ioctl_last = 0;
    if (fd != 66)
        kpanic("testdev test failed: ioctl(): Internal ID mismatch");

    ioctl_last = (int) mem;

    printk("testdev: IOCTL'd\n");
    return ioctl_last;
}

void testdev_init() {
    dev_char_t* devc = kzmalloc(sizeof(dev_char_t));

    devc->char_ops    = &test_ops;
    devc->internal_id = 66;

    dev_register_char("test", devc);
}