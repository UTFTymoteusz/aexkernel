#pragma once

#include "aex/klist.h"
#include "aex/mutex.h"

#include "aex/proc/task.h"

struct bqueue_entry {
    task_t* task;
    struct bqueue_entry* next;
};
typedef struct bqueue_entry bqueue_entry_t;

struct bqueue {
    bqueue_entry_t* first;
    bqueue_entry_t* last;
    mutex_t mutex;

    bool defunct;
};
typedef struct bqueue bqueue_t;

void io_create_bqueue(bqueue_t*);

void io_block(bqueue_t*);
void io_unblockall(bqueue_t*);

// Unblocks all processes in the queue and ensures that new tasks cannot get blocked on the queue
void io_defunct(bqueue_t*);

void io_sblock();
void io_sunblock(task_t*);