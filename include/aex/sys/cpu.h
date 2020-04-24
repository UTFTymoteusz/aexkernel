#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CURRENT_CPU 0

struct thread_context;
typedef struct thread_context thread_context_t;

#define CPU_TIMER_HZ 500

#include "cpu_int.h"

struct thread;
typedef struct thread thread_t;

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

// Fills up an existing thread_context_t with parameters that fit the desired behaviour
void cpu_fill_context(thread_context_t* context, bool kernelmode, void* entry, phys_addr paging_descriptor_addr);

void cpu_set_stack(thread_context_t* context, void* stack_ptr, size_t size);
void cpu_set_stacks(thread_t* thread, void* kernel_stack, size_t kstk_size, 
                void* user_stack, size_t ustk_size);
                
void cpu_dispose_stacks(thread_t* thread, size_t kstk_size, size_t ustk_size);