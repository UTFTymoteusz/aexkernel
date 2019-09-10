#pragma once

enum fs_inode_type {
    FS_INODE_TYPE_NULL = 0,
    FS_INODE_TYPE_DIRECTORY = 1,
    FS_INODE_TYPE_FILE = 2,
    FS_INODE_TYPE_MOUNT = 3,
    FS_INODE_TYPE_SYMLINK = 4,
    FS_INODE_TYPE_DEV = 5,
}

struct fs_inode_operations {
    
}

struct fs_inode {
    size_t uid;

    uint8_t type;
    void* data;
}
typedef struct fs_inode fs_inode_t;

struct fs_inode_file {
    uint64_t ptr[64];
    struct fs_inode_file* next;
}

struct fs_inode_directory {

}