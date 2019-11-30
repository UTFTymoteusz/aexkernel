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

extern void task_enter();
extern void task_switch_full();

task_t* task_current;
task_context_t* task_current_context;

task_t* idle_task;
task_t* task_queue;

size_t next_id = 1;
volatile size_t task_ticks = 0;
size_t task_count = 0;

void idle_task_loop() {
	while (true)
		waitforinterrupt();
}

task_t* task_create(process_t* process, bool kernelmode, void* entry, size_t page_dir_addr) {
    task_t* new_task = kmalloc(sizeof(task_t));
    task_context_t* new_context = kmalloc(sizeof(task_context_t));

    memset(new_task, 0, sizeof(task_t));
    memset(new_context, 0, sizeof(task_context_t));

    new_task->kernel_stack = mempg_alloc(mempg_to_pages(KERNEL_STACK_SIZE), NULL, 0x03) + KERNEL_STACK_SIZE;

    void* stack = mempg_alloc(mempg_to_pages(BASE_STACK_SIZE), process->ptracker, kernelmode ? 0x03 : 0x07);

    cpu_fill_context(new_context, kernelmode, entry, page_dir_addr);
    cpu_set_stack(new_context, stack, BASE_STACK_SIZE);

    new_task->stack_addr = stack;

    new_task->context = new_context;
    new_task->kernelmode = kernelmode;

    new_task->id = next_id++;
    new_task->next = NULL;
    new_task->status = TASK_STATUS_RUNNABLE;

    new_task->process = process;

    return new_task;
}

void task_dispose(task_t* task) {
    mempg_free(mempg_indexof(task->kernel_stack - KERNEL_STACK_SIZE, NULL), mempg_to_pages(KERNEL_STACK_SIZE), NULL);
    
    kfree(task->context);
    kfree(task);
}

void task_insert(task_t* task) {
    if (task->in_queue)
        kpanic("Attempt to insert an already-inserted task");

    task->in_queue = true;

    if (task_queue == NULL) {
        task->next = NULL;
        task->prev = NULL;

        task_queue = task;
        return;
    }
    task->next = task_queue;
    task_queue = task;

    (task->next)->prev = task;
    ++task_count;
}

void task_remove(task_t* task) {
    if (!(task->in_queue))
        kpanic("Attempt to remove an already-removed task");

    task->in_queue = false;

    if (task_queue == NULL)
        return;

    if (task_queue == task) {
        task_queue = task_queue->next;
        return;
    }
    if (task->next != NULL)
        (task->next)->prev = task->prev;;
        
    (task->prev)->next = task->next;
    --task_count;
}

void task_timer_tick() {
    ++task_ticks;
}

void task_switch_stage2() {
    task_t* next_task = task_current;
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
            case TASK_STATUS_BLOCKED:
                break;
            default:
                goto task_select_end;
        }
    }
    task_select_end:
    task_current = next_task;
    
    task_current_context = next_task->context;
    process_current = next_task->process;

    task_enter();
}

void syscall_sleep(long delay) {
    if (delay == -1) {
        task_remove(task_current);
        task_switch_full();
    }
    mutex_acquire(&(task_current->access));

    task_current->status = TASK_STATUS_SLEEPING;
    task_current->resume_after = task_ticks + (delay / (1000 / CPU_TIMER_HZ));

    mutex_release(&(task_current->access));
    task_switch_full();
}

void syscall_yield() {
    task_switch_full();
}

void syscall_exit(int status) {
    printf("Process %i (%s) exited with code %i\n", process_current->pid, process_current->name, status);
    process_kill(process_current->pid);
}

bool syscall_thexit() {
    return thread_kill(task_current->thread);
}

uint64_t syscall_getpid() {
    return process_current->pid;
}

uint64_t syscall_getthid() {
    return task_current->thread->id;
}

void syscall_proctest() {
    printf("syscall boi from userspace\n");
}

void task_init() {
    idle_task = task_create(process_current, true, idle_task_loop, cpu_get_kernel_page_dir());

    task_t* task0 = task_create(process_current, true, NULL, cpu_get_kernel_page_dir());
    task_insert(task0);

    task_current = task0;
    task_current_context = task0->context;

    syscalls[SYSCALL_SLEEP]    = syscall_sleep;
    syscalls[SYSCALL_YIELD]    = syscall_yield;
    syscalls[SYSCALL_EXIT]     = syscall_exit;
    syscalls[SYSCALL_THEXIT]   = syscall_thexit;
    syscalls[SYSCALL_PROCTEST] = syscall_proctest;
    syscalls[SYSCALL_GETPID]   = syscall_getpid;
    syscalls[SYSCALL_GETTHID]  = syscall_getthid;
}