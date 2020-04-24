#pragma once

#include "aex/event.h"
#include "aex/spinlock.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct cbuf {
    volatile size_t write_ptr, read_ptr;
    size_t size;

    spinlock_t  spinlock;
    event_t event;

    uint8_t* buffer;
};
typedef struct cbuf cbuf_t;

int cbuf_create(cbuf_t* cbuf, size_t size);

size_t cbuf_read(cbuf_t* cbuf, uint8_t* buffer, size_t len);
size_t cbuf_write(cbuf_t* cbuf, uint8_t* buffer, size_t len);

size_t cbuf_available(cbuf_t* cbuf);