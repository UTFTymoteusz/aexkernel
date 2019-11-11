#pragma once

#include "dev/cpu.h"

enum task_queue {
    TASK_QUEUE_RUNNABLE = 0,
    TASK_QUEUE_SLEEPING = 1,
    TASK_QUEUE_DEAD = 666,
};

enum task_status {
    TASK_STATUS_RUNNABLE = 0,
    TASK_STATUS_SLEEPING = 1,
    TASK_STATUS_BLOCKED = 3,
};

struct task {
    void* kernel_stack;
    void* stack_addr;
    void* paging_root;

    size_t id;
    struct process* process;
    struct thread*  thread;

    bool kernelmode;
    bool pass;

    bool io_blocked;

    uint8_t status;
    union {
        size_t resume_after;
    };
    struct task_context* context;
    struct task* next;
};
typedef struct task task_t;

task_t* task_current;
task_context_t* task_current_context;

task_t* idle_task;

task_t* task_queue;

volatile size_t task_ticks;
size_t task_count;

void task_init();

// Creates a task
task_t* task_create(struct process* process, bool kernelmode, void* entry, size_t page_dir_addr);

// This function loads, calculates task states and entries into a task. This must be preceeded by stage1 of the task switch.
void task_switch_stage2();

// Performs what stage1 (saving) and stage2 (loading, calculation, entry) should do in one call, allowing this to be used universally.
extern void task_switch_full();

// Inserts a task into the queue
void task_insert(task_t* task);

// Removes a task from the queue
void task_remove(task_t* task);

// Releases resources associated with a task
void task_dispose(task_t* task);