#pragma once

#include "page.c"

void page_assign(void* virt, void* phys, void* root, unsigned char flags);