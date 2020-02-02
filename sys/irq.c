#include "aex/aex.h"
#include "aex/cbufm.h"
#include "aex/mem.h"
#include "aex/spinlock.h"

#include "aex/proc/exec.h"
#include "aex/proc/proc.h"
#include "aex/proc/task.h"

#include <stddef.h>

#include "aex/sys/irq.h"

irq_func_t* irqs[24];
irq_func_t* irqs_imm[24];

task_t* irq_workers[IRQ_WORKER_AMOUNT];

spinlock_t irq_worker_spinlock = {
    .val  = 0,
    .name = "irq worker",
};

struct irq_queue {
    spinlock_t spinlock;
    uint8_t buffer[8192];

    uint16_t write_ptr;
};

struct irq_queue irqq = {
    .spinlock = {
        .val = 0,
        .name = "irq queue",
    },
    .write_ptr = 0,
};

size_t irqq_read(uint8_t* irq, size_t start) {
    size_t possible, w_off;

    while (true) {
        spinlock_wait(&(irqq.spinlock));

        w_off = irqq.write_ptr;
        if (w_off < start)
            w_off += sizeof(irqq.buffer);

        possible = sizeof(irqq.buffer) - start;

        if (possible > (w_off - start))
            possible = w_off - start;

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
    static int irq_worker_to_use = 0;

    spinlock_acquire(&(irqq.spinlock));

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

    if (irq_worker_to_use >= IRQ_WORKER_AMOUNT)
        irq_worker_to_use = 0;

    io_sunblock(irq_workers[irq_worker_to_use++]);

    spinlock_release(&(irqq.spinlock));
}

size_t irqq_available(size_t start) {
    spinlock_wait(&(irqq.spinlock));

    if (start == irqq.write_ptr)
        return 0;

    if (start > irqq.write_ptr)
        return (sizeof(irqq.buffer) - start) + irqq.write_ptr;

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
        spinlock_acquire(&irq_worker_spinlock);

        if (irqq_available(irq_last) == 0) {
            spinlock_release(&irq_worker_spinlock);
            irqq_wait(irq_last);
            continue;
        }
        irq_last = irqq_read(&irq, irq_last);

        spinlock_release(&irq_worker_spinlock);

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
        tid_t th_id = thread_create(KERNEL_PROCESS, irq_worker, true);
        if (process_lock(KERNEL_PROCESS)) {
            thread_t* th = thread_get(KERNEL_PROCESS, th_id);
            th->name = kmalloc(32);
            snprintf(th->name, 32, "IRQ Worker %li", i);

            task_set_priority(th->task, PRIORITY_CRITICAL);

            set_arguments(th->task, 1, i);

            irq_workers[i] = th->task;

            process_unlock(KERNEL_PROCESS);
        }
        thread_start(KERNEL_PROCESS, th_id);
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