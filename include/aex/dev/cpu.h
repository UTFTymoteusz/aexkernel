#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct task_context;
typedef struct task_context task_context_t;

#define CPU_TIMER_HZ 200

#include "aex/proc/task.h"

#include "cpu_int.h"

// Initializes the CPU into an usable state
void cpu_init();

// Gets the CPU vendor
static inline char* cpu_get_vendor(char buffer[16]);

// Halts the processor, there's no coming back from this!
static inline void halt();

static inline uint8_t inportb(uint16_t _port);
static inline void    outportb(uint16_t _port, uint8_t _data);

static inline uint16_t inportw(uint16_t _port);
static inline void     outportw(uint16_t _port, uint16_t _data);

static inline uint32_t inportd(uint16_t _port);
static inline void     outportd(uint16_t _port, uint32_t _data);

// Enables interrupts
void interrupts();

// Disables interrupts
void nointerrupts();

// Returns true if interrupts are enabled
bool checkinterrupts();

// Waits for any interrupt
void waitforinterrupt();

// Fills up an existing task_context_t with parameters that fit the desired behaviour
void cpu_fill_context(task_context_t* context, bool kernelmode, void* entry, phys_addr page_dir_addr);

void cpu_set_stack(task_context_t* context, void* stack_ptr, size_t size);

uint64_t cpu_get_kernel_page_dir();