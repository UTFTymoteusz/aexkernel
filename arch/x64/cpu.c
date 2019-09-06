#pragma once

#define CPU_ARCH "AMD64"
#define CPU_PAGE_SIZE 0x1000

struct regs {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err;
    uint64_t rip, cs, rflags, rsp, ss;
};

typedef size_t addr;

static inline int cpuid_string(int code, uint32_t where[4]) {
    asm volatile("cpuid":"=a"(*where),"=b"(*(where+1)),
                "=c"(*(where+2)),"=d"(*(where+3)):"a"(code));
    
    return (int)where[0];
}

const int cpu_id_reg_order[3] = {1, 3, 2};
char* cpu_get_vendor(char* ret) {
    ret[12] = 0;

    uint32_t where[4];
    cpuid_string(0, where);

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
            ret[i*4 + j] = (where[cpu_id_reg_order[i]] >> (j * 8)) & 0xFF;

    return ret;
}

unsigned char inportb(unsigned short _port)
{
	unsigned char rv;
	asm volatile("inb %0, %1" : "=a" (rv) : "dN" (_port));
	return rv;
}
void outportb(unsigned short _port, unsigned char _data)
{
	asm volatile("outb %0, %1" : : "dN" (_port), "a" (_data));
}

static inline void interrupts() {
    asm volatile("sti");
}
static inline void nointerrupts() {
    asm volatile("sti");
}

static inline void halt() {
    while (true)
        asm volatile("hlt");
}

#include "idt.c"
#include "irq.c"
#include "isr.c"

void cpu_init() {
    idt_init();
    isr_init();
    irq_init();
}