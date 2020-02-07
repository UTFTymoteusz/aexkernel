#pragma once

#include "aex/fs/fd.h"
#include "aex/kernel.h"
#include "aex/string.h"

#include "aex/proc/proc.h"
#include "aex/vals/ioctl_names.h"

enum fs_mode {
    FS_MODE_READ    = 0x0001,
    FS_MODE_WRITE   = 0x0002,
    FS_MODE_EXECUTE = 0x0004,
};

enum fs_type {
    FS_TYPE_NONE = 0x0000,
    FS_TYPE_FILE = 0x0001,
    FS_TYPE_DIR  = 0x0002,
};

struct hook_file_data {
    char* path;
    pid_t pid;

    int mode;
};
typedef struct hook_file_data hook_file_data_t;

struct finfo {
    int      dev_id;
    uint16_t type;

    uint64_t size;

    uint64_t block_size;
    uint64_t block_count;
};
typedef struct finfo finfo_t;

struct mount;
typedef struct mount mount_t;

struct filesystem {
    char name[24];

    int (*mount)  (mount_t* mount);
    int (*unmount)(mount_t* mount);

    int  (*open)(mount_t* mount, char* path, int mode);
    int64_t (*read) (mount_t* mount, int ifd, void* buffer, uint64_t len);
    int64_t (*write)(mount_t* mount, int ifd, void* buffer, uint64_t len);
    int64_t (*seek) (mount_t* mount, int ifd, int64_t offset, int mode);
    int (*close)(mount_t* mount, int ifd);

    int64_t (*ioctl)(mount_t* mount, int fd, int64_t code, void* data);

    int (*opendir)(mount_t* mount, char* path);
    int (*readdir)(mount_t* mount, int ifd, dentry_t* dentry_dst);

    int  (*finfo) (mount_t* mount, char* path, finfo_t* finfo);
    int  (*fdinfo)(mount_t* mount, int ifd, finfo_t* finfo);
};
typedef struct filesystem filesystem_t;

struct mount {
    int id;
    char* path;

    int dev_id;
    void* private_data;

    filesystem_t* fs;
};
typedef struct mount mount_t;

/*
 * Initializes the filesystem subsystem.
 */
void fs_init();

/*
 * Registers a filesystem.
 */
void fs_register(filesystem_t* fs);

/*
 * Mounts a device with the specified filesystem.
 */
int fs_mount(char* dev, char* path, char* type);

/*
 * Tries to open a file.
 */
int fs_open(char* path, int mode);

/*
 * Tries to open a directory.
 */
int fs_opendir(char* path);

/*
 * Returns information about a file.
 */
int fs_finfo(char* path, finfo_t* finfo);

/*
 * Counts the amount of segments in a path.
 */
#define path_count_segments(path) \
    ({ \
        __label__ done; \
        \
        char*  local = path; \
        size_t count = 0;    \
        \
        while (true) { \
            if (*local == '/') \
                local++;       \
            \
            char* ptr = strchrnul(local, '/'); \
            \
            if (ptr == local) \
                goto done;    \
            \
            int len = ptr - local + 1; \
            count++; \
            \
            local += len - 1; \
        } \
          \
    done: \
        count; \
    })

/*
 * Walks over a path, skipping over root. Keep in mind the level name length
 * limit of 255 (256 if you count the trailing null).
 */
#define path_walk(path, segment, left) \
    for (char* local = ({left = path_count_segments(path); path + 1;}); ({ \
        __label__ done; \
        \
        bool success = false; \
        \
        if (*local == '/') \
            local++;       \
        \
        char* ptr = strchrnul(local, '/'); \
        \
        if (ptr == local) \
            goto done;    \
        \
        int len = ptr - local + 1; \
        snprintf(segment, (len > 255) ? 255 : len, "%s", local); \
        \
        local += len - 1; \
        left--;           \
        success = true;   \
        \
    done: \
        success; \
    }); )