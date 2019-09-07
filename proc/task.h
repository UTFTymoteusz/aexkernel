#pragma once

#include "dev/cpu.h"
#include "proc/task.c"

struct task_descriptor;

struct task_descriptor* task_current;
struct task_context* task_current_context;

void task_init();
struct task_descriptor* task_create(bool kernelmode, void* entry, size_t page_dir_addr);