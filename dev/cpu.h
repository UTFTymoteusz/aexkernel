#pragma once

#include "cpu.c"

typedef size_t addr;

// Initializes the CPU into an usable state
void cpu_init();

// Gets the CPU vendor
char* cpu_get_vendor(char buffer[16]);

// Halts the processor, there's no coming back from this!
void halt();

// Enables interrupts
static inline void interrupts();

// Disables interrupts
static inline void nointerrupts();