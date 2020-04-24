#include "aex/rcode.h"
#include "aex/rcparray.h"
#include "aex/rcptable.h"
#include "aex/syscall.h"
#include "aex/fs/fd.h"
#include "aex/fs/fs.h"
#include "aex/proc/task.h"

#include <stdint.h>

extern rcptable(process_t) processes;

int64_t syscall_fopen(char* path, int mode) {
    process_t* process = rcptable_get(processes, CURRENT_PID);
    if (process == NULL)
        return -EINTR;

    char* _path = fs_to_absolute_path(path, process->cwd);

    int fd = fs_open(_path, mode);
    kfree(_path);

    IF_ERROR(fd) {
        rcptable_unref(processes, CURRENT_PID);
        return fd;
    }

    int lfd = rcparray_add(process->fiddies, fd);

    rcptable_unref(processes, CURRENT_PID);
    return lfd;
}

int64_t syscall_fread(int lfd, void* buffer, uint64_t len) {
    process_t* process = rcptable_get(processes, CURRENT_PID);
    if (process == NULL)
        return -EINTR;

    int* fd = rcparray_get(process->fiddies, lfd);
    if (fd == NULL) {
        rcptable_unref(processes, CURRENT_PID);
        return -EBADF;
    }

    int _fd = *fd;
    int64_t ret = fd_read(_fd, buffer, len);

    rcparray_unref(process->fiddies, lfd);
    rcptable_unref(processes, CURRENT_PID);

    return ret;
}

int64_t syscall_fwrite(int lfd, void* buffer, uint64_t len) {
    process_t* process = rcptable_get(processes, CURRENT_PID);
    if (process == NULL)
        return -EINTR;

    int* fd = rcparray_get(process->fiddies, lfd);
    if (fd == NULL) {
        rcptable_unref(processes, CURRENT_PID);
        return -EBADF;
    }

    int _fd = *fd;
    int64_t ret = fd_write(_fd, buffer, len);

    rcparray_unref(process->fiddies, lfd);
    rcptable_unref(processes, CURRENT_PID);

    return ret;
}

int64_t syscall_fseek(int lfd, int64_t offset, int mode) {
    process_t* process = rcptable_get(processes, CURRENT_PID);
    if (process == NULL)
        return -EINTR;

    int* fd = rcparray_get(process->fiddies, lfd);
    if (fd == NULL) {
        rcptable_unref(processes, CURRENT_PID);
        return -EBADF;
    }

    int _fd = *fd;
    int64_t ret = fd_seek(_fd, offset, mode);

    rcparray_unref(process->fiddies, lfd);
    rcptable_unref(processes, CURRENT_PID);

    return ret;
}

int64_t syscall_fclose(int lfd) {
    process_t* process = rcptable_get(processes, CURRENT_PID);
    if (process == NULL)
        return -EINTR;

    spinlock_acquire(&process->fiddie_lock);

    int* fd = rcparray_get(process->fiddies, lfd);
    if (fd == NULL) {
        rcptable_unref(processes, CURRENT_PID);
        spinlock_release(&process->fiddie_lock);
        return -EBADF;
    }

    int _fd = *fd;

    rcparray_unref(process->fiddies, lfd);
    rcparray_remove(process->fiddies, lfd);

    spinlock_release(&process->fiddie_lock);
    
    int ret = fd_close(_fd);

    rcptable_unref(processes, CURRENT_PID);
    return ret;
}

int64_t syscall_ioctl(int lfd, int64_t code, void* mem) {
    process_t* process = rcptable_get(processes, CURRENT_PID);
    if (process == NULL)
        return -EINTR;

    int* fd = rcparray_get(process->fiddies, lfd);
    if (fd == NULL) {
        rcptable_unref(processes, CURRENT_PID);
        return -EBADF;
    }

    int _fd = *fd;
    int64_t ret = fd_ioctl(_fd, code, mem);

    rcparray_unref(process->fiddies, lfd);
    rcptable_unref(processes, CURRENT_PID);

    return ret;
}

int64_t syscall_finfo(char* path, finfo_t* finfo) {
    process_t* process = rcptable_get(processes, CURRENT_PID);
    if (process == NULL)
        return -EINTR;

    char* _path = fs_to_absolute_path(path, process->cwd);
    int ret = fs_finfo(_path, finfo);

    kfree(_path);

    rcptable_unref(processes, CURRENT_PID);

    return ret;
}


int64_t syscall_opendir(char* path) {
    process_t* process = rcptable_get(processes, CURRENT_PID);
    if (process == NULL)
        return -EINTR;

    char* _path = fs_to_absolute_path(path, process->cwd);
    int fd = fs_opendir(_path);

    kfree(_path);

    IF_ERROR(fd) {
        rcptable_unref(processes, CURRENT_PID);
        return fd;
    }

    int lfd = rcparray_add(process->fiddies, fd);

    rcptable_unref(processes, CURRENT_PID);
    return lfd;
}

int64_t syscall_readdir(int lfd, dentry_t* dentry) {
    process_t* process = rcptable_get(processes, CURRENT_PID);
    if (process == NULL)
        return -EINTR;

    int* fd = rcparray_get(process->fiddies, lfd);
    if (fd == NULL) {
        rcptable_unref(processes, CURRENT_PID);
        return -EBADF;
    }

    int _fd = *fd;
    int ret = fd_readdir(_fd, dentry);

    rcparray_unref(process->fiddies, lfd);
    rcptable_unref(processes, CURRENT_PID);

    return ret;
}


void fs_syscall_init() {
    syscalls[SYSCALL_FOPEN]  = syscall_fopen;
    syscalls[SYSCALL_FREAD]  = syscall_fread;
    syscalls[SYSCALL_FWRITE] = syscall_fwrite;
    syscalls[SYSCALL_FSEEK]  = syscall_fseek;
    syscalls[SYSCALL_FCLOSE] = syscall_fclose;
    syscalls[SYSCALL_IOCTL]  = syscall_ioctl;

    syscalls[SYSCALL_FINFO]  = syscall_finfo;

    syscalls[SYSCALL_OPENDIR] = syscall_opendir;
    syscalls[SYSCALL_READDIR] = syscall_readdir;
}