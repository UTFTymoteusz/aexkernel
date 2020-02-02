#pragma once

#include "aex/io.h"

#include "aex/proc/proc.h"

struct dev_char {
    struct dev_file_ops* ops;
    int internal_id;

    thread_t* worker;
    spinlock_t   access;
};
typedef struct dev_char dev_char_t;

int  dev_register_char(char* name, struct dev_char* dev_char);
void dev_unregister_char(char* name);

int  dev_char_open (int id, file_t* file);
int  dev_char_read (int id, file_t* file, uint8_t* buffer, int len);
int  dev_char_write(int id, file_t* file, uint8_t* buffer, int len);
int  dev_char_close(int id, file_t* file);
long dev_char_ioctl(int id, file_t* file, long code, void* mem);