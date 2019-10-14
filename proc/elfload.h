#pragma once

#include "mem/pagetrk.h"

struct exec_data {
    void* entry;
    void* addr;
    void* phys_addr;

    uint64_t page_amount;
    uint64_t starting_frame;
};

int elf_load(char* path, struct exec_data* exec, page_tracker_t* tracker);