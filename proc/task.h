#pragma once

#include "dev/cpu.h"

#include "task.c"

enum task_queue;

struct task_descriptor;

struct task_descriptor* task_current;
struct task_context* task_current_context;

struct task_descriptor* idle_task;

struct task_descriptor* task_queue_runnable;
struct task_descriptor* task_queue_sleeping;

void task_init();

// Creates a task
struct task_descriptor* task_create(bool kernelmode, void* entry, size_t page_dir_addr);

// This function loads, calculates task states and entries into a task. This must be preceeded by stage1 of the task switch.
void task_switch_stage2();

// Performs what stage1 (saving) and stage2 (loading, calculation, entry) should do in one call, allowing this to be used universally.
extern void task_switch_full();

// Inserts a task into a queue
void task_insert(task_descriptor_t* task, int queue);

// Removes a task from a queue
void task_remove(task_descriptor_t* task, int queue);