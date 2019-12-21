#include "aex/aex.h"
#include "aex/cbufm.h"
#include "aex/mem.h"
#include "aex/mutex.h"

#include "aex/proc/exec.h"
#include "aex/proc/proc.h"
#include "aex/proc/task.h"

#include <stddef.h>
#include <stdio.h>

#include "aex/irq.h"

irq_func_t* irqs[24];
irq_func_t* irqs_imm[24];

task_t* irq_workers[IRQ_WORKER_AMOUNT];

mutex_t irq_worker_mutex = 0;

struct irq_queue {
    mutex_t mutex;
    uint8_t buffer[8192];

    uint16_t write_ptr;
};

struct irq_queue irqq = {
    .mutex     = 0,
    .write_ptr = 0,
};

size_t irqq_read(uint8_t* irq, size_t start) {
    size_t possible, wdoff;

    while (true) {
        mutex_wait(&(irqq.mutex));

        wdoff = irqq.write_ptr;
        if (wdoff < start)
            wdoff += sizeof(irqq.buffer);

        possible = sizeof(irqq.buffer) - start;

        if (possible > (wdoff - start))
            possible = wdoff - start;

        if (possible > 1)
            possible = 1;

        if (possible == 0) {
            io_sblock();
            continue;
        }
        *irq = irqq.buffer[start];

        start += possible;
        if (start == sizeof(irqq.buffer))
            start = 0;

        break;
    }
    return start;
}

void irqq_write(uint8_t irq) {
    mutex_acquire(&(irqq.mutex));

    int len = 1;
    int amnt;
    while (len > 0) {
        amnt = sizeof(irqq.buffer) - irqq.write_ptr;
        if (amnt > len)
            amnt = len;

        len -= amnt;

        irqq.buffer[irqq.write_ptr] = irq;
        irqq.write_ptr += amnt;

        if (irqq.write_ptr == sizeof(irqq.buffer))
            irqq.write_ptr = 0;
    }

    for (int i = 0; i < IRQ_WORKER_AMOUNT; i++)
        io_sunblock(irq_workers[i]);

    mutex_release(&(irqq.mutex));
}

size_t irqq_available(size_t start) {
    mutex_wait(&(irqq.mutex));

    if (start == irqq.write_ptr)
        return 0;

    if (start > irqq.write_ptr)
        return (sizeof(irqq.buffer) - start) + irqq.write_ptr;
    else
        return irqq.write_ptr - start;
}

void irqq_wait(size_t start) {
    while (true) {
        if (irqq_available(start) == 0) {
            io_sblock();
            continue;
        }
        break;
    }
}

void irq_worker(UNUSED int id) {
    static size_t irq_last = 0;

    uint8_t irq;

    while (true) {
        mutex_acquire(&irq_worker_mutex);

        if (irqq_available(irq_last) == 0) {
            mutex_release(&irq_worker_mutex);
            irqq_wait(irq_last);
            continue;
        }
        irq_last = irqq_read(&irq, irq_last);

        mutex_release(&irq_worker_mutex);

        irq_func_t* current = irqs[irq];
        while (current != NULL) {
            current->func();
            current = current->next;
        }
    }
}

void irq_enqueue(uint8_t irq) {
    irq_func_t* current = irqs_imm[irq];
    while (current != NULL) {
        current->func();
        current = current->next;
    }
    irqq_write(irq);
}

void irq_initsys() {
    for (size_t i = 0; i < IRQ_WORKER_AMOUNT; i++) {
        thread_t* th = thread_create(process_current, irq_worker, true);
        th->name = kmalloc(32);
        sprintf(th->name, "IRQ Worker %i", i);

        task_set_priority(th->task, PRIORITY_CRITICAL);

        set_arguments(th->task, i);
        thread_start(th);

        irq_workers[i] = th->task;
    }
}

long irq_install(int irq, void* func) {
    irq_func_t* current = irqs[irq];
    if (current == NULL) {
        irqs[irq] = kmalloc(sizeof(irq_func_t));
        irqs[irq]->func = func;
        irqs[irq]->next = NULL;
        return 0;
    }

    while (current->next != NULL)
        current = current->next;
        
    current->next = kmalloc(sizeof(irq_func_t));
    current = current->next;
    current->func = func;
    current->next = NULL;

    return 0;
}

long irq_install_immediate(int irq, void* func) {
    irq_func_t* current = irqs_imm[irq];
    if (current == NULL) {
        irqs_imm[irq] = kmalloc(sizeof(irq_func_t));
        irqs_imm[irq]->func = func;
        irqs_imm[irq]->next = NULL;
        return 0;
    }

    while (current->next != NULL) {
        if (current->func == func)
            return -1;

        current = current->next;
    }
    
    current->next = kmalloc(sizeof(irq_func_t));
    current = current->next;
    current->func = func;
    current->next = NULL;

    return 0;
}

long irq_remove(int irq, void* func) {
    irq_func_t* current = irqs_imm[irq];
    if (current == NULL)
        return -1;

    irq_func_t* prev = NULL;

    while (current->next != NULL) {
        if (current->func == func) {
            if (prev == NULL) {
                irqs_imm[irq] = current->next;
                return 0;
            }
            prev->next = current->next;
            return 0;
        }
        prev    = current;
        current = current->next;
    }
    return -1;
}