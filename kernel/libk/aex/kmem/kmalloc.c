#include "mem/pool.h"

#include <stdint.h>

void* kmalloc(uint32_t size) {
    return mempo_alloc(size);
}