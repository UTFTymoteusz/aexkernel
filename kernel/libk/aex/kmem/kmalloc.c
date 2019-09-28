#pragma once

#include "mem/pool.h"

void* kmalloc(uint32_t size) {
    return mempo_alloc(size);
}