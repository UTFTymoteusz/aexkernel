#pragma once

#include "aex/cbufm.h"

#define IRQ_WORKER_AMOUNT 4

struct irq_func {
    void (*func)();
    struct irq_func* next;
};
typedef struct irq_func irq_func_t;

void irq_initsys();

void irq_enqueue(uint8_t irq);

long irq_install(int irq, void* func);
long irq_install_immediate(int irq, void* func);
long irq_remove(int irq, void* func);