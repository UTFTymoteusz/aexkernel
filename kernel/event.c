#include "aex/proc/task.h"
#include "aex/proc/tqueue.h"

#include "aex/event.h"

int event_wait(event_t* event) {
    spinlock_acquire(&event->lock);
    
    if (task_tshould_exit(CURRENT_TID)) {
        spinlock_release(&event->lock);

        printk("event: Received abort\n");
        return -1;
    }

    tqueue_add(&event->wait_queue);
    spinlock_release(&event->lock);

    if (task_tcan_yield())
        task_tyield();

    if (task_tshould_exit(CURRENT_TID)) {
        spinlock_release(&event->lock);

        printk("event: Received abort\n");
        return -1;
    }
    return 0;
}

void event_trigger(event_t* event) {
    spinlock_acquire(&event->lock);

    //__sync_add_and_fetch(&event->counter, 1);
    tqueue_wakeup(&event->wait_queue);

    spinlock_release(&event->lock);
}

void event_trigger_all(event_t* event) {
    spinlock_acquire(&event->lock);

    tqueue_wakeup_all(&event->wait_queue);
    //__sync_add_and_fetch(&event->counter, count);

    spinlock_release(&event->lock);
}

void event_defunct(event_t* event) {
    spinlock_acquire(&event->lock);

    tqueue_wakeup_all(&event->wait_queue);
    event->counter = 0x7FFFFFF;

    spinlock_release(&event->lock);
}