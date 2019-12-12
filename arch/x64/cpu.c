#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "aex/proc/task.h"

//#include "aex/dev/cpu.h"
#include "cpu_int.h"

extern void* PML4;

void cpu_fill_context(task_context_t* context, bool kernelmode, void* entry, cpu_addr page_dir_addr) {
    if (kernelmode) {
        context->cs = 0x08;
        context->ss = 0x10;
    }
    else {
        context->cs = 0x23;
        context->ss = 0x1B;
    }
    context->cr3    = page_dir_addr;
    context->rflags = 0x202;
    context->rip = (uint64_t) entry;
}

void cpu_set_stack(task_context_t* context, void* ptr, size_t size) {
    context->rbp = (size_t) ptr;
    context->rsp = (size_t) ptr + size;
}

uint64_t cpu_get_kernel_page_dir() {
    return (uint64_t) &PML4;
}

struct idt_entry {
   uint16_t offset0;
   uint16_t selector;
   uint8_t  ist;      
   uint8_t  type_attr;
   uint16_t offset1;
   uint32_t offset2;
   uint32_t zero;
} __attribute((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;

void idt_set_entry(uint16_t index, void* ptr, uint8_t attributes) {
    size_t offset = (size_t) ptr;

    idt[index].offset0 = offset & 0xFFFF;
    idt[index].offset1 = (offset & 0xFFFF0000) >> 16;
    idt[index].offset2 = (offset & 0xFFFFFFFF00000000) >> 32;

    idt[index].selector = 0x08;
    idt[index].ist = 0;
    idt[index].type_attr = attributes;
    idt[index].zero = 0;
}

extern void idt_load();
void idt_init() {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (cpu_addr)&idt;
    
    memset(&idt, 0, sizeof(struct idt_entry) * 256);

    idt_load();
}

extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

void isr_init() {
    idt_set_entry(0, isr0, 0x8E);
    idt_set_entry(1, isr1, 0x8E);
    idt_set_entry(2, isr2, 0x8E);
    idt_set_entry(3, isr3, 0x8E);
    idt_set_entry(4, isr4, 0x8E);
    idt_set_entry(5, isr5, 0x8E);
    idt_set_entry(6, isr6, 0x8E);
    idt_set_entry(7, isr7, 0x8E);
    idt_set_entry(8, isr8, 0x8E);
    idt_set_entry(9, isr9, 0x8E);
    idt_set_entry(10, isr10, 0x8E);
    idt_set_entry(11, isr11, 0x8E);
    idt_set_entry(12, isr12, 0x8E);
    idt_set_entry(13, isr13, 0x8E);
    idt_set_entry(14, isr14, 0x8E);
    idt_set_entry(15, isr15, 0x8E);
    idt_set_entry(16, isr16, 0x8E);
    idt_set_entry(17, isr17, 0x8E);
    idt_set_entry(18, isr18, 0x8E);
    idt_set_entry(19, isr19, 0x8E);
    idt_set_entry(20, isr20, 0x8E);
    idt_set_entry(21, isr21, 0x8E);
    idt_set_entry(22, isr22, 0x8E);
    idt_set_entry(23, isr23, 0x8E);
    idt_set_entry(24, isr24, 0x8E);
    idt_set_entry(25, isr25, 0x8E);
    idt_set_entry(26, isr26, 0x8E);
    idt_set_entry(27, isr27, 0x8E);
    idt_set_entry(28, isr28, 0x8E);
    idt_set_entry(29, isr29, 0x8E);
    idt_set_entry(30, isr30, 0x8E);
    idt_set_entry(31, isr31, 0x8E);
}

extern void syscall_init_asm();

void cpu_init() {
    idt_init();
    isr_init();
    irq_init();

    syscall_init_asm();

    timer_init(CPU_TIMER_HZ);
}