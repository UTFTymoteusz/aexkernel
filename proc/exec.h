#pragma once

#include "proc/task.h"

void setup_entry_caller(void* addr);
void init_entry_caller(task_t* task, void* entry);

void set_arguments(task_t* task, ...);