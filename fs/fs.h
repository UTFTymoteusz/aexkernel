#pragma once

#include "fs.c"

enum fs_flag;

struct filesystem;
struct filesystem_mounted;

struct klist filesystems;

void fs_init();

void fs_register(struct filesystem* fssys);

int fs_mount(char* dev, char* path, char* type);