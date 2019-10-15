#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "proc/task.h"

#include "dev/cpu.h"

extern void* tss_ptr;
struct tss* cpu_tss = (struct tss*)&tss_ptr;

void cpu_fill_context(task_context_t* context, bool kernelmode, void* entry, cpu_addr page_dir_addr) {
    if (kernelmode) {
        context->cs = 0x08;
        context->ss = 0x10;
    }
    else {
        context->cs = 0x23;
        context->ss = 0x1B;
    }
    //printf("Addr 0x%x\n", page_dir_addr);

    //for (volatile long i = 0; i < 400000000; i++) ;

    context->cr3    = page_dir_addr;
    context->rflags = 0x202;
    context->rip = (uint64_t)entry;
}

void cpu_set_stack(task_context_t* context, void* ptr, size_t size) {
    context->rbp = (size_t)ptr;
    context->rsp = (size_t)ptr + size;
}

extern void* PML4;

uint64_t cpu_get_kernel_page_dir() {
    return (uint64_t)&PML4;
}

struct idt_entry {
   uint16_t offset0;
   uint16_t selector;
   uint8_t ist;      
   uint8_t type_attr;
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
    size_t offset = (size_t)ptr;

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

extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

void irq_set_mask(uint16_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8)
        port = 0x21;
    else {
        port = 0xA1;
        irq -= 8;
    }
    value = inportb(port) | (1 << irq);
    outportb(port, value);
}

void irq_init() {
	outportb(0x20, 0x11);
	outportb(0xA0, 0x11);
	outportb(0x21, 0x20);
	outportb(0xA1, 0x28);
	outportb(0x21, 0x04);
	outportb(0xA1, 0x02);
	outportb(0x21, 0x01);
	outportb(0xA1, 0x01);
	outportb(0x21, 0x0);
	outportb(0xA1, 0x0);

    idt_set_entry(32, irq0, 0x8E);
    idt_set_entry(33, irq1, 0x8E);
    idt_set_entry(34, irq2, 0x8E);
    idt_set_entry(35, irq3, 0x8E);
    idt_set_entry(36, irq4, 0x8E);
    idt_set_entry(37, irq5, 0x8E);
    idt_set_entry(38, irq6, 0x8E);
    idt_set_entry(39, irq7, 0x8E);
    idt_set_entry(40, irq8, 0x8E);
    idt_set_entry(41, irq9, 0x8E);
    idt_set_entry(42, irq10, 0x8E);
    idt_set_entry(43, irq11, 0x8E);
    idt_set_entry(44, irq12, 0x8E);
    idt_set_entry(45, irq13, 0x8E);
    idt_set_entry(46, irq14, 0x8E);
    idt_set_entry(47, irq15, 0x8E);

    for (int i = 1; i < 15; i++)
        irq_set_mask(i);
}

void irq_handler(struct regs* r) {
    printf("IRQ %i\n", r->int_no - 32);

    if (r->int_no >= 40)
        outportb(0xA0, 0x20);

    outportb(0x20, 0x20);
}

void task_tss() {
    cpu_tss->rsp0 = (uint64_t)task_current->kernel_stack;
}

void timer_set_hz(int hz) {
    int divisor = 1193180 / hz;       /* Calculate our divisor */

    outportb(0x43, 0x36);             /* Set our command byte 0x36 */
    outportb(0x40, divisor & 0xFF);   /* Set low byte of divisor */
    outportb(0x40, divisor >> 8);     /* Set high byte of divisor */
}

void timer_init(int hz) {
    timer_set_hz(hz);
}

extern void syscall_init_asm();

void cpu_init() {
    idt_init();
    isr_init();
    irq_init();

    memset((void*)cpu_tss, 0, sizeof(struct tss));

    syscall_init_asm();

    timer_init(CPU_TIMER_HZ);
}