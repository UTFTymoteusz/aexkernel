#pragma once

#include "mem/pool.h"

void kfree(void* block) {
    mem_pool_unalloc(block);
}