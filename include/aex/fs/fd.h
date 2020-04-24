#pragma once

#include <stddef.h>
#include <stdint.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

struct dentry {
    uint16_t type;
    char name[256];
};
typedef struct dentry dentry_t;

struct file_ops {
    int64_t (*read) (int fd, void* buffer, uint64_t len);
    int64_t (*write)(int fd, void* buffer, uint64_t len);
    int64_t (*seek) (int fd, int64_t offset, int mode);
    int     (*close)(int fd);
    
    int64_t (*ioctl)(int fd, int64_t code, void* data);

    int (*readdir)(int fd, dentry_t* dentry_dst);

    int (*dup)(int fd);
};
typedef struct file_ops file_ops_t;

struct file_descriptor {
    int ifd;
    file_ops_t ops;
};
typedef struct file_descriptor file_descriptor_t;

/*
 * Adds a file descriptor.
 */
int fd_add(file_descriptor_t fd);

/*
 * Reads data from an open file descriptor. Returns the actual amount of bytes
 * that have been read, or an error.
 */
int64_t fd_read(int fd, void* buffer, uint64_t len);

/*
 * Writes data to an open file descriptor. Returns the actual amount of bytes
 * that have been written, or an error.
 */
int64_t fd_write(int fd, void* buffer, uint64_t len);

/*
 * Sets the fd file cursor position. If mode is 0, offset is counted from the
 * beginning. If mode is 1, offset is counted from the cursor position. If mode
 * is 2, offset is counted from the cursor position (cursor + offset).
 */
int64_t fd_seek(int fd, int64_t offset, int mode);

/*
 * ioctl()'s a file descriptor.
 */
int64_t fd_ioctl(int fd, int64_t code, void* data);

/*
 * Closes a file descriptor.
 */
int fd_close(int fd);

/*
 * Reads the next directory entry in the opened fd. Returns 0 on success and 
 * -1 upon reaching the end.
 */
int fd_readdir(int fd, dentry_t* dentry_dst);

/*
 * Duplicates an open file descriptor.
 */
int fd_dup(int fd);