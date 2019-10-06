#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CPU_TIMER_HZ 500

#include "cpu_int.h"

struct regs {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute((packed));

struct task_context {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute((packed));
typedef struct task_context task_context_t;

struct tss {
    uint32_t reserved0;
    uint64_t rsp0, rsp1, rsp2;
    uint64_t reserved1;

    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;

    uint64_t reserved2;
    uint32_t reserved3;
} __attribute((packed));

typedef size_t addr;

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

// Waits for any interrupt
void waitforinterrupt();

// Fills up an existing task_context_t with parameters that fit the desired behaviour
void cpu_fill_context(task_context_t* context, bool kernelmode, void* entry, addr page_dir_addr);

void cpu_set_stack(task_context_t* context, void* stack_ptr, size_t size);