#pragma once

#include "proc/proc.h"

enum chr_rq_type {
    CHR_OPEN    = 0,
    CHR_READ    = 1,
    CHR_WRITE   = 2,
    CHR_CLOSE   = 3,
    CHR_IOCTL   = 4,
};

struct chr_request {
    uint8_t type;

    int len;

    uint8_t* buffer;
    long     response;

    thread_t* thread;
    volatile bool done;

    struct chr_request* next;
};
typedef struct chr_request chr_request_t;

struct dev_char {
    struct dev_file_ops* ops;
    int internal_id;

    thread_t* worker;
    mutex_t   access;
    chr_request_t* io_queue;
    chr_request_t* last_crq;
};
typedef struct dev_char dev_char_t;

int  dev_register_char(char* name, struct dev_char* dev_char);
void dev_unregister_char(char* name);

int  dev_char_open(int id);
int  dev_char_read(int id, uint8_t* buffer, int len);
int  dev_char_write(int id, uint8_t* buffer, int len);
int  dev_char_close(int id);
long dev_char_ioctl(int id, long code, void* mem);