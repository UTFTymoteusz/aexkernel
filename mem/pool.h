#pragma once

#include "stddef.h"
#include "stdint.h"

struct mem_pool;
typedef struct mem_pool mem_pool_t;

void mempo_init();
void mempo_enum_root();