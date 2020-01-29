#include <stddef.h>
#include <stdint.h>

#include "aex/aex.h"
#include "aex/debug.h"
#include "aex/kernel.h"

struct stackframe {
    struct stackframe* rbp;
    uint64_t rip;
} PACKED;
typedef struct stackframe stackframe_t;

void debug_print_registers() {
    uint64_t reg;
    uint16_t reg_s;
    
    asm volatile("mov %0, cr0" : "=r"(reg));
    printk("CR0: 0x%016lX ", reg);
    printk("CR1: #ud :) ");

    asm volatile("mov %0, cr2" : "=r"(reg));
    printk("CR2: 0x%016lX ", reg);

    asm volatile("mov %0, cr3" : "=r"(reg));
    printk("CR3: 0x%016lX\n", reg);


    asm volatile("pushf; pop %0" : "=r"(reg));
    printk("RFLAGS: 0x%016lX\n", reg);

    
    asm volatile("mov %0, cs" : "=r"(reg_s));
    printk("CS: 0x%04X ", reg_s);

    asm volatile("mov %0, ds" : "=r"(reg_s));
    printk("DS: 0x%04X ", reg_s);
    
    asm volatile("mov %0, es" : "=r"(reg_s));
    printk("ES: 0x%04X ", reg_s);

    asm volatile("mov %0, ss" : "=r"(reg_s));
    printk("SS: 0x%04X ", reg_s);
    
    asm volatile("mov %0, fs" : "=r"(reg_s));
    printk("FS: 0x%04X ", reg_s);

    asm volatile("mov %0, gs" : "=r"(reg_s));
    printk("GS: 0x%04X\n", reg_s);

    asm volatile("mov %0, rsp" : "=r"(reg));
    printk("RSP: 0x%016lX\n", reg);
}

void debug_stacktrace() {
    stackframe_t* stkfr;
    asm volatile("mov %0, rbp" : "=r"(stkfr));
    
    printk("Stack trace:\n");
    while (stkfr != NULL) {
        printk(" 0x%016lX\n", stkfr->rip);
        stkfr = stkfr->rbp;
    }
}