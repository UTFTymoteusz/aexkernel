#include "aex/rcparray.h"
#include "aex/rcode.h"

#include <stdint.h>

#include "aex/fs/fs.h"
#include "aex/fs/fd.h"

rcparray(file_descriptor_t) fds = {0};

int fd_add(file_descriptor_t fd) {
    return rcparray_add(fds, fd);
}

int64_t fd_read(int fd, void* buffer, uint64_t len) {
    file_descriptor_t* _fd = rcparray_get(fds, fd);
    if (_fd == NULL)
        return ERR_INV_ARGUMENTS;

    if (_fd->ops.read == NULL)
        printk(PRINTK_WARN "fd->ops.read is NULL\n");

    int64_t ret = _fd->ops.read(_fd->ifd, buffer, len);
    rcparray_unref(fds, fd);
    
    return ret;
}

int64_t fd_write(int fd, void* buffer, uint64_t len) {
    file_descriptor_t* _fd = rcparray_get(fds, fd);
    if (_fd == NULL)
        return ERR_INV_ARGUMENTS;

    if (_fd->ops.write == NULL)
        printk(PRINTK_WARN "fd->ops.write is NULL\n");

    int64_t ret = _fd->ops.write(_fd->ifd, buffer, len);
    rcparray_unref(fds, fd);
    
    return ret;
}

int64_t fd_seek(int fd, int64_t len, int mode) {
    file_descriptor_t* _fd = rcparray_get(fds, fd);
    if (_fd == NULL)
        return ERR_INV_ARGUMENTS;

    if (_fd->ops.seek == NULL)
        printk(PRINTK_WARN "fd->ops.seek is NULL\n");

    int64_t ret = _fd->ops.seek(_fd->ifd, len, mode);
    rcparray_unref(fds, fd);
    
    return ret;
}

int fd_close(int fd) {
    file_descriptor_t* _fd = rcparray_get(fds, fd);
    if (_fd == NULL)
        return ERR_INV_ARGUMENTS;

    if (_fd->ops.close == NULL)
        printk(PRINTK_WARN "fd->ops.close is NULL\n");

    file_descriptor_t copy = *_fd;
        
    rcparray_unref (fds, fd);
    rcparray_remove(fds, fd);

    return copy.ops.close(copy.ifd);
}

int64_t fd_ioctl(int fd, int64_t code, void* data) {
    file_descriptor_t* _fd = rcparray_get(fds, fd);
    if (_fd == NULL)
        return ERR_INV_ARGUMENTS;

    if (_fd->ops.seek == NULL)
        printk(PRINTK_WARN "fd->ops.ioctl is NULL\n");

    int64_t ret = _fd->ops.ioctl(_fd->ifd, code, data);
    rcparray_unref(fds, fd);
    
    return ret;
}

int fd_readdir(int fd, dentry_t* dentry_dst) {
    file_descriptor_t* _fd = rcparray_get(fds, fd);
    if (_fd == NULL)
        return ERR_INV_ARGUMENTS;

    int64_t ret = _fd->ops.readdir(_fd->ifd, dentry_dst);
    rcparray_unref(fds, fd);
    
    return ret;
}