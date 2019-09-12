#pragma once

#define CPU_TIMER_HZ 200

#include "cpu.c"

struct task_context;
typedef struct task_context task_context_t;

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

// Waits for any interrupt
static inline void waitforinterrupt();

// Fills up an existing task_context_t with parameters that fit the desired behaviour
void cpu_fill_context(task_context_t* context, bool kernelmode, void* entry, addr page_dir_addr);

void cpu_set_stack(task_context_t* context, void* stack_ptr, size_t size);