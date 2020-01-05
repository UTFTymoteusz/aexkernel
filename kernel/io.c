#include "aex/mem.h"
#include "aex/mutex.h"
#include "aex/string.h"
#include "aex/time.h"

#include "aex/proc/proc.h"
#include "aex/proc/task.h"

#include "aex/io.h"

void io_create_bqueue(bqueue_t* bqueue) {
    memset(bqueue, 0, sizeof(bqueue_t));
}

void io_block(bqueue_t* bqueue) {
    mutex_acquire(&(bqueue->mutex));
    mutex_acquire(&(task_current->access));

    if (bqueue->defunct) {
        mutex_release(&(task_current->access));
        mutex_release(&(bqueue->mutex));
        return;
    }
    
    bool was_busy = task_current->thread->added_busy;
    if (was_busy)
        process_release(task_current->process->pid);

    bool disable = checkinterrupts();
    if (disable)
        nointerrupts();

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

    mutex_release(&(task_current->access));
    mutex_release(&(bqueue->mutex));

    if (disable)
        interrupts();

    yield();
    
    if (was_busy)
        process_use(task_current->process->pid);
}

void io_unblockall(bqueue_t* bqueue) {
    mutex_acquire(&(bqueue->mutex));

    if (bqueue->first == NULL) {
        mutex_release(&(bqueue->mutex));
        return;
    }
    bool disable = checkinterrupts();
    if (disable)
        nointerrupts();

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

    mutex_release(&(bqueue->mutex));
    if (disable)
        interrupts();
}

void io_defunct(bqueue_t* bqueue) {
    mutex_acquire(&(bqueue->mutex));

    bqueue->defunct = true;

    if (bqueue->first == NULL) {
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

    mutex_release(&(bqueue->mutex));
}

void io_sblock() {
    bool disable = checkinterrupts();
    if (disable)
        nointerrupts();
        
    task_current->status = TASK_STATUS_BLOCKED;
    task_remove(task_current);

    if (disable)
        interrupts();

    yield();
}

void io_sunblock(task_t* task) {
    if (task->status != TASK_STATUS_BLOCKED)
        return;

    bool disable = checkinterrupts();
    if (disable)
        nointerrupts();
        
    task->status = TASK_STATUS_RUNNABLE;
    task_insert(task);
    
    if (disable)
        interrupts();
}