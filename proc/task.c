#pragma once

#include <string.h>

#include "aex/kmem.h"

#include "dev/cpu.h"

#define BASE_STACK_SIZE 128

struct task_descriptor {
    bool kernelmode;
    size_t id;

    struct task_context* context;

    struct task_descriptor* next;
};
typedef struct task_descriptor task_descriptor_t;

task_descriptor_t* task_current;
task_context_t* task_current_context;

task_descriptor_t* task0;
task_descriptor_t* idle_task;

size_t next_id = 1;

void bigbong() {
	while (true) {
		printf("bigbong B\n");
		waitforinterrupt();
	}
}

void idle_task_loop() {
	while (true)
		waitforinterrupt();
}

task_descriptor_t* task_find_last() {
    task_descriptor_t* task = idle_task;

    //static char boibuffer[24];
    //printf("first task: %s\n", itoa(task0->id, boibuffer, 10));

    while (task->next != NULL)
        task = task->next;

    //printf("last task: %s\n", itoa(task->id, boibuffer, 10));

    return task;
}

task_descriptor_t* task_create(bool kernelmode, void* entry, size_t page_dir_addr) {
    task_descriptor_t* new_task = kmalloc(sizeof(task_descriptor_t));
    task_context_t* new_context = kmalloc(sizeof(task_context_t));

    memset(new_task, 0, sizeof(task_descriptor_t));
    memset(new_context, 0, sizeof(task_context_t));

    cpu_fill_context(new_context, kernelmode, entry, page_dir_addr);
    cpu_set_stack(new_context, kmalloc(BASE_STACK_SIZE), BASE_STACK_SIZE);

    //static char boibuffer[24];
    //printf("task created with entry: 0x%s\n", itoa((uint64_t)entry, boibuffer, 16));

    new_task->context = new_context;
    new_task->kernelmode = kernelmode;

    new_task->id = next_id++;
    new_task->next = NULL;

    return new_task;
}
void task_insert(task_descriptor_t* task) {
    task_find_last()->next = task;
}

void dump() {
    static char boibuffer[24];
    printf("id: %s\n", itoa((uint32_t)(task_current->id), boibuffer, 10));

    printf("rax: 0x%s ", itoa((uint32_t)(task_current_context->rax), boibuffer, 16));
    printf("rbx: 0x%s ", itoa((uint32_t)(task_current_context->rbx), boibuffer, 16));
    printf("rcx: 0x%s ", itoa((uint32_t)(task_current_context->rcx), boibuffer, 16));
    printf("rdx: 0x%s ", itoa((uint32_t)(task_current_context->rdx), boibuffer, 16));
    printf("\n");
    printf("rsp: 0x%s ", itoa((uint32_t)(task_current_context->rsp), boibuffer, 16));
    printf("rbp: 0x%s ", itoa((uint32_t)(task_current_context->rbp), boibuffer, 16));
    printf("\n");
    printf("rip: 0x%s ", itoa((uint32_t)(task_current_context->rip), boibuffer, 16));
    printf("cs: 0x%s ", itoa((uint32_t)(task_current_context->cs), boibuffer, 16));
    printf("rfl: 0x%s ", itoa((uint32_t)(task_current_context->rflags), boibuffer, 16));
    printf("rsp: 0x%s ", itoa((uint32_t)(task_current_context->rsp), boibuffer, 16));
    printf("ss: 0x%s ", itoa((uint32_t)(task_current_context->ss), boibuffer, 16));
    printf("\n\n");
}

extern void task_enter();
void task_switch() {

    // Task state has been saved, hopefully

    task_current = task_current->next;

    if (task_current == NULL)
        task_current = idle_task;

    task_current_context = task_current->context;

    //dump();
    task_enter();
}

void task_init() {

    idle_task = task_create(true, idle_task_loop, 0);

    task0 = task_create(true, NULL, 0);
    task_insert(task0);

    //static char boibuffer[24];
    //printf("bigbong: 0x%s\n", itoa((uint64_t)bigbong, boibuffer, 16));

    task_current = task0;
    task_current_context = task0->context;
}