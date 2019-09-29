#pragma once

#include "dev/cpu.h"

#include "task.c"

enum task_queue;

struct task_descriptor;
typedef struct task_descriptor task_descriptor_t;

task_descriptor_t* task_current;
task_context_t* task_current_context;

task_descriptor_t* idle_task;

task_descriptor_t* task_queue_runnable;
task_descriptor_t* task_queue_sleeping;
task_descriptor_t* task_queue_dead;

void task_init();

// Creates a task
task_descriptor_t* task_create(bool kernelmode, void* entry, size_t page_dir_addr);

// This function loads, calculates task states and entries into a task. This must be preceeded by stage1 of the task switch.
void task_switch_stage2();

// Performs what stage1 (saving) and stage2 (loading, calculation, entry) should do in one call, allowing this to be used universally.
extern void task_switch_full();

// Inserts a task into a queue
void task_insert(task_descriptor_t* task, int queue);

// Removes a task from a queue
void task_remove(task_descriptor_t* task, int queue);

void syscall_sleep(long delay);
void syscall_yield();