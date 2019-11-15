#include "aex/kmem.h"
#include "aex/mutex.h"
#include "aex/time.h"

#include "proc/proc.h"
#include "proc/task.h"

#include <string.h>

#include "io.h"

void io_create_bqueue(bqueue_t* bqueue) {
    memset(bqueue, 0, sizeof(bqueue_t));
}

void io_block(bqueue_t* bqueue) {
    mutex_acquire(&(bqueue->mutex));
    mutex_acquire(&(task_current->access));
    nointerrupts();

    process_release(task_current->process);

    bqueue_entry_t* entry = kmalloc(sizeof(bqueue_entry_t));
    entry->task = task_current;
    entry->next = NULL;

    if (bqueue->last == NULL) {
        bqueue->first = entry;
        bqueue->last  = entry;
    }
    else {
        bqueue->last->next = entry;
        bqueue->last = entry;
    }
    task_current->status = TASK_STATUS_BLOCKED;
    task_remove(task_current);

    interrupts();
    mutex_release(&(task_current->access));
    mutex_release(&(bqueue->mutex));

    yield();
    process_use(task_current->process);
}

void io_unblockall(bqueue_t* bqueue) {
    if (bqueue->first == NULL)
        return;

    mutex_acquire(&(bqueue->mutex));
    nointerrupts();

    if (bqueue->first == NULL) {
        interrupts();
        mutex_release(&(bqueue->mutex));
        return;
    }
    bqueue_entry_t* entry = bqueue->first;
    bqueue_entry_t* nx;
    while (entry != NULL) {
        if (entry->task->status != TASK_STATUS_DEAD) {
            entry->task->status = TASK_STATUS_RUNNABLE;
            task_insert(entry->task);
        }
        nx = entry->next;

        kfree(entry);
        entry = nx;
    }
    bqueue->first = NULL;
    bqueue->last  = NULL;

    interrupts();
    mutex_release(&(bqueue->mutex));
}

void io_sblock() {
    nointerrupts();
    task_current->status = TASK_STATUS_BLOCKED;
    task_remove(task_current);
    interrupts();

    yield();
}

void io_sunblock(task_t* task) {
    if (task->status != TASK_STATUS_BLOCKED)
        return;

    nointerrupts();
    task->status = TASK_STATUS_RUNNABLE;
    task_insert(task);
    interrupts();
}