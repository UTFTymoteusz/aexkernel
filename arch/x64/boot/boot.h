#pragma once

#include "boot/multiboot.h"

bool find_boot_device(multiboot_info_t* mbt, char* rootname);