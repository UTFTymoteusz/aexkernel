#pragma once

#include "aex/mem.h"

#include "aex/proc/task.h"
#include <stdarg.h>

struct exec_data {
    void* entry;
    void* kernel_entry;
    
    phys_addr phys_addr;

    size_t init_code_addr;

    uint64_t page_amount;
};

size_t args_memlen(char* args[]);
size_t args_count(char* args[]);

void setup_entry_caller(void* addr, size_t proc_addr, char* args[]);
void init_entry_caller (task_t* task, void* entry, size_t proc_addr, int arg_count);

void set_arguments(task_t* task, int arg_count, ...);
void set_varguments(task_t* task, int arg_count, va_list args);

int exec_load(char* path, char* args[], struct exec_data* exec, pagemap_t** proot);

int exec_register_executor(char* name, int (*executor_function)(char* path, char* args[], struct exec_data* exec, pagemap_t** proot));
int exec_unregister_executor(char* name);