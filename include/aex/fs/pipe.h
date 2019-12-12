#pragma once

#include "aex/cbuf.h"

#include "aex/fs/fs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct pipe {
    bool broken;
    cbuf_t cbuf;
};
typedef struct pipe pipe_t;

int fs_pipe_create(file_t* reader, file_t* writer, size_t size);

int fs_pipe_read(file_t* file, uint8_t* buffer, int len);
int fs_pipe_write(file_t* file, uint8_t* buffer, int len);