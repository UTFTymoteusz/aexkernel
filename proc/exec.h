#pragma once

#include "proc/task.h"

void setup_entry_caller(void* addr);
void init_entry_caller(task_descriptor_t* task, void* entry);