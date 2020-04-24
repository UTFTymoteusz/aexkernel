#pragma once

#include "aex/spinlock.h"
#include "aex/proc/tqueue.h"

struct event {
    spinlock_t lock;

    volatile int counter;

    tqueue_t wait_queue;
};
typedef struct event event_t;

/*
 * Waits for the event to trigger. This blocks the thread and yields it's 
 * timeslice. Yielding can fail when a spinlock is held, so in that case you 
 * must yield yourself. Returns 0 on success and -1 if the abort signal was 
 * received.
 */
int event_wait(event_t* event);

/*
 * Triggers an event, waking up the first thread on the queue.
 */
void event_trigger(event_t* event);

/*
 * Triggers an event, waking up the every thread on the queue.
 */
void event_trigger_all(event_t* event);

/*
 * Triggers an event, waking up the every thread on the queue and ensures that
 * no threads can wait after.
 */
void event_defunct(event_t* event);