#include "aex/spinlock.h"
#include "aex/proc/task.h"
#include "aex/proc/tqueue.h"

void tqueue_add(tqueue_t* tqueue) {
    spinlock_acquire(&tqueue->lock);

    tqueue_entry_t* entry = kzmalloc(sizeof(tqueue_entry_t));
    entry->thread = CURRENT_TID;

    if (tqueue->first == NULL)
        tqueue->first = entry;
    else
        tqueue->last->next = entry;

    tqueue->last = entry;

    task_tset_status(CURRENT_TID, THREAD_STATUS_BLOCKED);
    spinlock_release(&tqueue->lock);
}

bool tqueue_wakeup(tqueue_t* tqueue) {
    spinlock_acquire(&tqueue->lock);

    if (tqueue->first == NULL) {
        spinlock_release(&tqueue->lock);
        return false;
    }

    tqueue_entry_t* entry = tqueue->first;
    if (task_tget_status(entry->thread) == THREAD_STATUS_BLOCKED)
        task_tset_status(entry->thread, THREAD_STATUS_RUNNABLE);

    tqueue->first = entry->next;
    kfree(entry);

    spinlock_release(&tqueue->lock);
    return true;
}

int tqueue_wakeup_all(tqueue_t* tqueue) {
    spinlock_acquire(&tqueue->lock);

    int count = 0;

    tqueue_entry_t* entry = tqueue->first;
    tqueue_entry_t* next;

    while (entry != NULL) {
        if (task_tget_status(entry->thread) == THREAD_STATUS_BLOCKED)
            task_tset_status(entry->thread, THREAD_STATUS_RUNNABLE);

        next = entry->next;
        kfree(entry);

        ++count;
        entry = next;
    }
    tqueue->first = NULL;
    tqueue->last  = NULL;

    spinlock_release(&tqueue->lock);
    return count;
}