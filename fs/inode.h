#pragma once

#include "inode.c"

struct inode;
typedef struct inode inode_t;

inode_t* inode_alloc();