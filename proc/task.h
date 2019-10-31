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
    TASK_STATUS_YIELDED = 2,
};

struct task_descriptor {
    void* kernel_stack;
    void* paging_root;

    size_t id;
    struct process* process;

    bool kernelmode;
    bool pass;

    uint8_t status;
    union {
        size_t resume_after;
    };
    struct task_context* context;
    struct task_descriptor* next;
};
typedef struct task_descriptor task_descriptor_t;

task_descriptor_t* task_current;
task_context_t* task_current_context;

task_descriptor_t* idle_task;

task_descriptor_t* task_queue;

volatile size_t task_ticks;
size_t task_count;

void task_init();

// Creates a task
task_descriptor_t* task_create(struct process* process, bool kernelmode, void* entry, size_t page_dir_addr);

// This function loads, calculates task states and entries into a task. This must be preceeded by stage1 of the task switch.
void task_switch_stage2();

// Performs what stage1 (saving) and stage2 (loading, calculation, entry) should do in one call, allowing this to be used universally.
extern void task_switch_full();

// Inserts a task into a queue
void task_insert(task_descriptor_t* task);

// Removes a task from a queue
void task_remove(task_descriptor_t* task);