#pragma once

#include "aex/spinlock.h"

struct tqueue_entry {
    int thread;
    struct tqueue_entry* next;
};
typedef struct tqueue_entry tqueue_entry_t;

struct tqueue {
    tqueue_entry_t* first;
    tqueue_entry_t* last;

    spinlock_t lock;
};
typedef struct tqueue tqueue_t;

/*
 * Adds the current thread to the queue, blocks it and tries to yield. If yielding
 * fails then you must yield yourself.
 */
void tqueue_add(tqueue_t* tqueue);

/*
 * Wakes up the first thread in the queue and removes it from the queue.
 * Returns true if any thread was woken up.
 */
bool tqueue_wakeup(tqueue_t* tqueue);

/*
 * Wakes up all threads on the queue. Returns the number of threads woken up.
 */
int tqueue_wakeup_all(tqueue_t* tqueue);