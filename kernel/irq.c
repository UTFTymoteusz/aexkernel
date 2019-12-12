#include "aex/cbufm.h"
#include "aex/mem.h"
#include "aex/mutex.h"

#include "aex/proc/exec.h"
#include "aex/proc/proc.h"
#include "aex/proc/task.h"

#include <stddef.h>
#include <stdio.h>

#include "aex/irq.h"

void (*(irqs[16][IRQ_HOOK_AMOUNT]))();

cbufm_t irq_queue;
mutex_t irq_worker_mutex = 0;

void irq_worker(int id) {
    static size_t irq_last = 0;

    uint8_t irq;

    while (true) {
        cbufm_wait(&irq_queue, irq_last);

        mutex_acquire(&irq_worker_mutex);

        if (cbufm_available(&irq_queue, irq_last) == 0) {
            mutex_release(&irq_worker_mutex);
            continue;
        }
        irq_last = cbufm_read(&irq_queue, &irq, irq_last, 1);

        mutex_release(&irq_worker_mutex);

        for (int i = 0; i < IRQ_HOOK_AMOUNT; i++)
            if (irqs[irq][i] != NULL)
                irqs[irq][i]();
    }
}

void irq_enqueue(uint8_t irq) {
    cbufm_write(&irq_queue, &irq, 1);
}

void irq_initsys() {
    for (int i = 0; i < 16; i++)
        for (int j = 0; j < IRQ_HOOK_AMOUNT; j++)
            irqs[i][j] = NULL;

    cbufm_create(&irq_queue, 8192);

    for (size_t i = 0; i < IRQ_WORKER_AMOUNT; i++) {
        thread_t* th = thread_create(process_current, irq_worker, true);
        th->name = kmalloc(32);
        sprintf(th->name, "IRQ Worker %i", i);

        task_set_priority(th->task, PRIORITY_CRITICAL);

        set_arguments(th->task, i);
        thread_start(th);
    }
}

long irq_install(int irq, void* func) {
    for (int i = 0; i < 32; i++)
        if (irqs[irq][i] == NULL) {
            irqs[irq][i] = func;
            return i;
        }

    return -1;
}

long irq_remove(int irq, void* func) {
    for (int i = 0; i < 32; i++)
        if (irqs[irq][i] == func) {
            irqs[irq][i] = NULL;
            return i;
        }
        
    return 0;
}