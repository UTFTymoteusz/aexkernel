#pragma once

#include <stddef.h>

#include "mem/pagetrk.h"

// Loads an ELF executable from the specified path, allocates memory and fills the exec data struct. Returns a result code.
int elf_load(char* path, char* args[], struct exec_data* exec, page_root_t* proot);