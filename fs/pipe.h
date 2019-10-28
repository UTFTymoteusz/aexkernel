#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct pipe {
    uint8_t* reader_pos;
    uint8_t* writer_pos;

    bool broken;

    size_t size;
    uint8_t* buffer;
};
typedef struct pipe pipe_t;