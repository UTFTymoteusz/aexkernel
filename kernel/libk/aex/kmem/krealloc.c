#include "mem/pool.h"

#include <stdint.h>

void* krealloc(void* block, uint32_t size) {
    return mempo_realloc(block, size);
}