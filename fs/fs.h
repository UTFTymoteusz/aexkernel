#pragma once

#include "fs.c"

enum fs_flag;
enum fs_record_type;

struct filesystem;
struct filesystem_mount;

struct klist filesystems;

void fs_init();

void fs_register(struct filesystem* fssys);

int fs_mount(char* dev, char* path, char* type);
int fs_get_mount(char* path, struct filesystem_mount** mount);

int fs_get_inode(char* path, inode_t** inode);
void fs_retire_inode(inode_t* inode);

int fs_count(char* path);
int fs_list(char* path, dentry_t* dentries, int max);