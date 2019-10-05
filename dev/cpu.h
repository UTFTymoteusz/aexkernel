#pragma once

#define CPU_TIMER_HZ 500

#include "cpu.c"

struct task_context;
typedef struct task_context task_context_t;

typedef size_t addr;

// Initializes the CPU into an usable state
void cpu_init();

// Gets the CPU vendor
char* cpu_get_vendor(char buffer[16]);

// Halts the processor, there's no coming back from this!
inline void halt();


inline uint8_t inportb(uint16_t _port);
inline void    outportb(uint16_t _port, uint8_t _data);

inline uint16_t inportw(uint16_t _port);
inline void     outportw(uint16_t _port, uint16_t _data);

inline uint32_t inportd(uint16_t _port);
inline void     outportd(uint16_t _port, uint32_t _data);

// Enables interrupts
static inline void interrupts();

// Disables interrupts
static inline void nointerrupts();

// Waits for any interrupt
static inline void waitforinterrupt();

// Fills up an existing task_context_t with parameters that fit the desired behaviour
void cpu_fill_context(task_context_t* context, bool kernelmode, void* entry, addr page_dir_addr);

void cpu_set_stack(task_context_t* context, void* stack_ptr, size_t size);