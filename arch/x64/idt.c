#pragma once

#include <string.h>

typedef size_t addr;

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
    uint32_t base;
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
    idtp.base = (addr)&idt;
    
    memset(&idt, 0, sizeof(struct idt_entry) * 256);

    idt_load();
}