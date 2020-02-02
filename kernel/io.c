#include "aex/mem.h"
#include "aex/spinlock.h"
#include "aex/string.h"
#include "aex/time.h"

#include "aex/proc/proc.h"
#include "aex/proc/task.h"

#include "aex/io.h"

void io_create_bqueue(bqueue_t* bqueue) {
    memset(bqueue, 0, sizeof(bqueue_t));
    bqueue->spinlock.val  = 0;
    bqueue->spinlock.name = NULL;
}

void io_block(bqueue_t* bqueue) {
    spinlock_acquire(&(bqueue->spinlock));

    if (bqueue->defunct) {
        spinlock_release(&(bqueue->spinlock));
        return;
    }
    bqueue_entry_t* entry = kmalloc(sizeof(bqueue_entry_t));
    entry->task = task_current;
    entry->next = NULL;

    if (bqueue->last == NULL)
        bqueue->first = entry;
    else
        bqueue->last->next = entry;

    bqueue->last = entry;

    task_current->status = TASK_STATUS_BLOCKED;

    spinlock_release(&(bqueue->spinlock));
    yield();
}

void io_unblockall(bqueue_t* bqueue) {
    spinlock_acquire(&(bqueue->spinlock));

    if (bqueue->first == NULL) {
        spinlock_release(&(bqueue->spinlock));
        return;
    }
    bqueue_entry_t* entry = bqueue->first;
    bqueue_entry_t* nx;
    
    while (entry != NULL) {
        if (entry->task->status != TASK_STATUS_DEAD)
            entry->task->status = TASK_STATUS_RUNNABLE;

        nx = entry->next;

        kfree(entry);
        entry = nx;
    }
    bqueue->first = NULL;
    bqueue->last  = NULL;

    spinlock_release(&(bqueue->spinlock));
}

void io_defunct(bqueue_t* bqueue) {
    spinlock_acquire(&(bqueue->spinlock));

    bqueue->defunct = true;

    if (bqueue->first == NULL) {
        spinlock_release(&(bqueue->spinlock));
        return;
    }
    bqueue_entry_t* entry = bqueue->first;
    bqueue_entry_t* nx;
    while (entry != NULL) {
        if (entry->task->status == TASK_STATUS_BLOCKED)
            entry->task->status = TASK_STATUS_RUNNABLE;

        nx = entry->next;

        kfree(entry);
        entry = nx;
    }
    bqueue->first = NULL;
    bqueue->last  = NULL;

    spinlock_release(&(bqueue->spinlock));
}

void io_sblock() {
    task_current->status = TASK_STATUS_BLOCKED;
    yield();
}

void io_sunblock(task_t* task) {
    if (task->status != TASK_STATUS_BLOCKED)
        return;

    task->status = TASK_STATUS_RUNNABLE;
}