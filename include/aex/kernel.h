#pragma once

#include <stdint.h>

#define PRINTK_TIME 0x0001

// Tells printk to skip prepending any data. Useful for printing out lists. Output will be padded to fit in line with the rest of printed data.
#define PRINTK_BARE "^0"

#define PRINTK_OK   "^1"
#define PRINTK_WARN "^2"
#define PRINTK_INIT "^4"

uint32_t get_printk_flags();
void set_printk_flags(uint32_t flags);

int printk(const char* format, ...);
int sprintf(char* dst, const char* restrict format, ...);