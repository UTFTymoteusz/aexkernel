#pragma once

#include "mem/pool.h"

void kfree(void* block) {
    mempo_unalloc(block);
}