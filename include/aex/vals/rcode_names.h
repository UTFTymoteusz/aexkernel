#pragma once

enum aex_return_code {
    EPERM   =  1, // Operation not permitted
    ENOENT  =  2, // No such file or directory
    ENOPS   =  3, // No such process
    EINTR   =  4, // Interrupted system call
    EIO     =  5, // I/O error
    EINVAL  =  6, // Invalid argument
    EBADF   =  7, // Invalid file descriptor
    EBADEXE =  8, // Invalid executable
    ENOSYS  =  9, // Not implemented
    ENODEV  = 10, // No such device
    ENOTDIR = 11, // Not a directory
    EISDIR  = 12, // Is a directory
    EROFS   = 13, // Read-only file system
    ERANGE  = 14, // Math result not representable
    EBUSY   = 15, // Device or resource busy
    ENAMETOOLONG = 16, // File name too long
};