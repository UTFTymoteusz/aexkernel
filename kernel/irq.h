#pragma once

#include "aex/cbufm.h"

#define IRQ_WORKER_AMOUNT 4
#define IRQ_HOOK_AMOUNT 16

void irq_initsys();

void irq_enqueue(uint8_t irq);

long irq_install(int irq, void* func);
long irq_remove(int irq, void* func);