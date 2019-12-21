#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#include "aex/aex.h"
#include "aex/debug.h"

struct stackframe {
    struct stackframe* rbp;
    uint64_t rip;
} PACKED;
typedef struct stackframe stackframe_t;

void debug_print_registers() {
    uint64_t reg;
    
    asm volatile("mov %0, cr0" : "=r"(reg));
    printf("CR0: 0x%x ", reg);
    printf("CR1: #ud :) ");

    asm volatile("mov %0, cr2" : "=r"(reg));
    printf("CR2: 0x%x ", reg);

    asm volatile("mov %0, cr3" : "=r"(reg));
    printf("CR3: 0x%x\n", reg);


    asm volatile("pushf; pop %0" : "=r"(reg));
    printf("RFLAGS: 0x%x\n", reg);

    
    asm volatile("mov %0, cs" : "=r"(reg));
    printf("CS: 0x%x ", reg);

    asm volatile("mov %0, ds" : "=r"(reg));
    printf("DS: 0x%x ", reg);
    
    asm volatile("mov %0, es" : "=r"(reg));
    printf("ES: 0x%x ", reg);

    asm volatile("mov %0, ss" : "=r"(reg));
    printf("SS: 0x%x ", reg);
    
    asm volatile("mov %0, fs" : "=r"(reg));
    printf("FS: 0x%x ", reg);

    asm volatile("mov %0, gs" : "=r"(reg));
    printf("GS: 0x%x\n", reg);

    asm volatile("mov %0, rsp" : "=r"(reg));
    printf("RSP: 0x%x\n", reg & 0xFFFFFFFFFFFF);
}

void debug_stacktrace() {
    stackframe_t* stkfr;
    asm volatile("mov %0, rbp" : "=r"(stkfr));
    
    printf("Stack trace:\n");
    while (stkfr != NULL) {
        printf(" 0x%x\n", stkfr->rip & 0xFFFFFFFFFFFF);
        stkfr = stkfr->rbp;
    }
}