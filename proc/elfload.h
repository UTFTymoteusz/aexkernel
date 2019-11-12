#pragma once

#include <stddef.h>

#include "mem/pagetrk.h"

struct exec_data {
    void* entry;
    void* pentry;
    
    void* addr;
    void* phys_addr;

    uint64_t page_amount;
    uint64_t starting_frame;
};

// Loads an ELF executable from the specified path, allocates memory and fills the exec data struct. Returns a result code.
int elf_load(char* path, struct exec_data* exec, page_tracker_t* tracker);