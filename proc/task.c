#include <stdio.h>
#include <string.h>

#include "aex/kmem.h"

#include "dev/cpu.h"

#include "kernel/sys.h"
#include "kernel/syscall.h"

#include "mem/page.h"

#include "task.h"
#include "proc.h"

#define BASE_STACK_SIZE 8192
#define KERNEL_STACK_SIZE 8192

/* TODO: Revamp the list thing */

extern void task_enter();
extern void task_switch_full();

task_descriptor_t* task_current;
task_context_t* task_current_context;

task_descriptor_t* task0;
task_descriptor_t* idle_task;

task_descriptor_t* task_queue;

size_t next_id = 1;
volatile size_t task_ticks = 0;
size_t task_count = 0;

void idle_task_loop() {
	while (true)
		waitforinterrupt();
}

task_descriptor_t* task_create(struct process* process, bool kernelmode, void* entry, size_t page_dir_addr) {
    task_descriptor_t* new_task = kmalloc(sizeof(task_descriptor_t));
    task_context_t* new_context = kmalloc(sizeof(task_context_t));

    memset(new_task, 0, sizeof(task_descriptor_t));
    memset(new_context, 0, sizeof(task_context_t));

    new_task->kernel_stack = mempg_alloc(mempg_to_pages(KERNEL_STACK_SIZE), process->ptracker, 0x03) + KERNEL_STACK_SIZE;

    void* stack;
    if (kernelmode)
        stack = mempg_alloc(mempg_to_pages(BASE_STACK_SIZE), process->ptracker, 0x03);
    else
        stack = mempg_alloc(mempg_to_pages(BASE_STACK_SIZE), process->ptracker, 0x07);

    cpu_fill_context(new_context, kernelmode, entry, page_dir_addr);
    cpu_set_stack(new_context, stack, BASE_STACK_SIZE);

    //printf("stack 0x%x\n", (size_t)(stack + BASE_STACK_SIZE) & 0xFFFFFFFFFFFF);

    new_task->context = new_context;
    new_task->kernelmode = kernelmode;

    new_task->id = next_id++;
    new_task->next = NULL;
    new_task->status = TASK_STATUS_RUNNABLE;

    new_task->process = process;

    return new_task;
}

void task_insert(task_descriptor_t* task) {
    task_descriptor_t* ctask = task_queue;
    if (ctask == NULL) {
        task_queue = task;
        return;
    }
    task->next = NULL;
    while (true) {
        if (ctask->next == task)
            return;
        if (ctask->next == NULL)
            break;

        ctask = ctask->next;
    }
    ++task_count;

    ctask->next = task;
}

void task_remove(task_descriptor_t* task) {
    kpanic("Attempt to call task_remove()");
}

void task_timer_tick() {
    ++task_ticks;
}

void task_switch_stage2() {
    if (task_current->pass == true)
        task_current->pass = false;
        
    task_descriptor_t* next_task = task_current;
    size_t iter = 0;
    size_t ticks = task_ticks;

    while (true) {
        next_task = next_task->next;

        if (iter++ > task_count) {
            next_task = idle_task;
            break;
        }
        if (next_task == NULL)
            next_task = task_queue;

        switch (next_task->status) {
            case TASK_STATUS_SLEEPING:
                if (ticks >= next_task->resume_after) {
                    next_task->status = TASK_STATUS_RUNNABLE;
                    goto task_select_end;
                }
                break;
            case TASK_STATUS_YIELDED:
                next_task->status = TASK_STATUS_RUNNABLE;
                break;
            default:
                goto task_select_end;
        }
    }
    task_select_end:
    task_current = next_task;
    
    task_current_context = next_task->context;
    process_current = next_task->process;

    //printf("task %i ", task_current->id);

    task_enter();
}

void syscall_sleep(long delay) {
    if (delay == -1) {
        task_remove(task_current);
        task_switch_full();
    }
    task_current->status = TASK_STATUS_SLEEPING;
    task_current->resume_after = task_ticks + (delay / (1000 / CPU_TIMER_HZ));
    task_current->pass = true;

    task_switch_full();
}

void syscall_yield() {
    task_current->status = TASK_STATUS_YIELDED;
    task_switch_full();
}

void syscall_proctest() {
    printf("syscall boi from userspace\n");
}

void task_init() {
    idle_task = task_create(process_current, true, idle_task_loop, cpu_get_kernel_page_dir());

    task0 = task_create(process_current, true, NULL, cpu_get_kernel_page_dir());
    task_insert(task0);

    task_current = task0;
    task_current_context = task0->context;

    syscalls[SYSCALL_SLEEP]    = syscall_sleep;
    syscalls[SYSCALL_YIELD]    = syscall_yield;
    syscalls[SYSCALL_PROCTEST] = syscall_proctest;
}