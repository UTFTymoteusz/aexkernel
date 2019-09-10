#pragma once

#include "mem/pool.h"

void* kmalloc(uint32_t size) {
    return mem_pool_alloc(size);
}