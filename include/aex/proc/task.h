#pragma once

#include "aex/mutex.h"

#include <stdbool.h>
#include <stddef.h>

#include "aex/dev/cpu.h"

struct process;

enum proc_priority {
    PRIORITY_CRITICAL = 0,
    PRIORITY_HIGH   = 3,
    PRIORITY_NORMAL = 4,
    PRIORITY_LOW    = 5,
};

enum task_status {
    TASK_STATUS_RUNNABLE = 0,
    TASK_STATUS_SLEEPING = 1,
    TASK_STATUS_BLOCKED  = 3,
    TASK_STATUS_DEAD     = 4,
};

#define TASK_QUEUE_COUNT 3

struct task {
    void* kernel_stack;
    void* stack_addr;
    void* paging_root;

    size_t id;

    struct process* process;
    struct thread*  thread;

    bool kernelmode;
    bool in_queue;

    uint8_t priority;

    mutex_t access;

    volatile uint8_t status;
    union {
        size_t resume_after;
    };
    struct task_context* context;

    struct task* next;
    struct task* prev;
};
typedef struct task task_t;

task_t* task_current;
task_context_t* task_current_context;

task_t* idle_task;

volatile size_t task_ticks;
double task_ms_per_tick;

// Creates a task.
task_t* task_create(struct process* process, bool kernelmode, void* entry, size_t page_dir_addr);

// This function loads, calculates task states and entries into a task. This must be preceeded by stage1 of the task switch.
void task_switch_stage2();

// Performs what stage1 (saving) and stage2 (loading, calculation, entry) should do in one call, allowing this to be used universally.
extern void task_switch_full();

// Inserts a task into the queue.
void task_insert(task_t* task);

// Removes a task from the queue.
void task_remove(task_t* task);

// Releases resources associated with a task.
void task_dispose(task_t* task);

void task_set_priority(task_t* task, uint8_t priority);
void task_put_to_sleep(task_t* task, size_t delay);