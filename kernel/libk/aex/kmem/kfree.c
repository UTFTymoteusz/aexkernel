#include "mem/pool.h"

#include <stdint.h>

void kfree(void* block) {
    mempo_unalloc(block);
}