#pragma once

#include "aex/proc/task.h"
#include <stdarg.h>

size_t args_memlen(char* args[]);
size_t args_count(char* args[]);

void setup_entry_caller(void* addr, size_t proc_addr, char* args[]);
void init_entry_caller (task_t* task, void* entry, size_t proc_addr, int arg_count);

void set_arguments(task_t* task, int arg_count, ...);
void set_varguments(task_t* task, int arg_count, va_list args);