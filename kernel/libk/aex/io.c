#include "aex/kmem.h"
#include "aex/mutex.h"
#include "aex/time.h"

#include "proc/task.h"

#include <string.h>

#include "io.h"

void io_create_bqueue(bqueue_t* bqueue) {
    memset(bqueue, 0, sizeof(bqueue_t));
}

void io_block(bqueue_t* bqueue) {
    nointerrupts();
    mutex_acquire(&(bqueue->mutex));

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

    mutex_release(&(bqueue->mutex));
    interrupts();
    yield();
}
void io_unblockall(bqueue_t* bqueue) {
    if (bqueue->first == NULL)
        return;

    mutex_acquire(&(bqueue->mutex));

    if (bqueue->first == NULL) {
        mutex_release(&(bqueue->mutex));
        return;
    }
    nointerrupts();

    bqueue_entry_t* entry = bqueue->first;
    bqueue_entry_t* nx;
    while (entry != NULL) {
        entry->task->status = TASK_STATUS_RUNNABLE;
        task_insert(entry->task);
        nx = entry->next;

        kfree(entry);
        entry = nx;
    }
    bqueue->first = NULL;
    bqueue->last  = NULL;

    mutex_release(&(bqueue->mutex));
    interrupts();
}