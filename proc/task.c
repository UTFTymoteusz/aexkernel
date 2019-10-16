#include <stdio.h>
#include <string.h>

#include "aex/kmem.h"

#include "dev/cpu.h"

#include "kernel/sys.h"
#include "kernel/syscall.h"

#include "task.h"

#define BASE_STACK_SIZE 128
#define KERNEL_STACK_SIZE 8192

/* TODO: Revamp the list thing */

enum task_queue;

struct task_descriptor;
typedef struct task_descriptor task_descriptor_t;

struct process* process_current;

extern void task_enter();
extern void task_switch_full();

task_descriptor_t* task_current;
task_context_t* task_current_context;

task_descriptor_t* task0;
task_descriptor_t* idle_task;

task_descriptor_t* task_queue_runnable;
task_descriptor_t* task_queue_sleeping;
task_descriptor_t* task_queue_dead;

size_t next_id = 1;

void idle_task_loop() {
	while (true)
		waitforinterrupt();
}

task_descriptor_t* task_create(bool kernelmode, void* entry, size_t page_dir_addr) {
    task_descriptor_t* new_task = kmalloc(sizeof(task_descriptor_t));
    task_context_t* new_context = kmalloc(sizeof(task_context_t));

    memset(new_task, 0, sizeof(task_descriptor_t));
    memset(new_context, 0, sizeof(task_context_t));

    cpu_fill_context(new_context, kernelmode, entry, page_dir_addr);
    cpu_set_stack(new_context, kmalloc(BASE_STACK_SIZE), BASE_STACK_SIZE);

    new_task->kernel_stack = (void*)((size_t)kmalloc(KERNEL_STACK_SIZE) + KERNEL_STACK_SIZE);

    new_task->context = new_context;
    new_task->kernelmode = kernelmode;

    new_task->id = next_id++;
    new_task->next = NULL;

    return new_task;
}

void task_insert(task_descriptor_t* task, int queue) {
    task->next = NULL;

    switch (queue) {
        case TASK_QUEUE_RUNNABLE:
            if (task_queue_runnable == NULL) {
                task_queue_runnable = task;
                return;
            }
            task->next = task_queue_runnable;
            task_queue_runnable = task;
            break;
        case TASK_QUEUE_SLEEPING:
            if (task_queue_sleeping == NULL) {
                task_queue_sleeping = task;
                return;
            }
            task->next = task_queue_sleeping;
            task_queue_sleeping = task;
            break;
        case TASK_QUEUE_DEAD:
            if (task_queue_dead == NULL) {
                task_queue_dead = task;
                return;
            }
            task->next = task_queue_dead;
            task_queue_dead = task;
            break;
    }
}

void task_remove(task_descriptor_t* task, int queue) {
    task_descriptor_t* ctask = NULL;

    switch (queue) {
        case TASK_QUEUE_RUNNABLE:
            ctask = task_queue_runnable;
            if (task == ctask) {
                task_queue_runnable = task_queue_runnable->next;
                return;
            }
            break;
        case TASK_QUEUE_SLEEPING:
            ctask = task_queue_sleeping;
            if (task == ctask) {
                task_queue_sleeping = task_queue_sleeping->next;
                return;
            }
            break;
        case TASK_QUEUE_DEAD:
            ctask = task_queue_dead;
            if (task == ctask) {
                task_queue_dead = task_queue_dead->next;
                return;
            }
            break;
    }
    while (ctask != NULL) {
        if (ctask->next == task) {
            ctask->next = task->next;
            task->next = NULL;

            return;
        }
        ctask = ctask->next;
    }
}

void task_timer_tick() {
    task_descriptor_t* task_s = task_queue_sleeping;
    size_t cnt = 0;

    while (task_s != NULL) {
        task_s->sreg_a--;

        if (task_s->sreg_a == 0) {
            task_remove(task_s, TASK_QUEUE_SLEEPING);
            task_insert(task_s, TASK_QUEUE_RUNNABLE);
        }
        task_s = task_s->next;

        cnt++;
        if (cnt > 43)
            printf("xdxd");
    }
}

void task_switch_stage2() {
    if (task_current->pass == true) {
        task_current->pass = false;

        task_current = task_queue_runnable;
    }
    else
        task_current = task_current->next;

    if (task_current == NULL)
        task_current = task_queue_runnable;

    if (task_current == NULL)
        task_current = idle_task;

    task_current_context = task_current->context;
    process_current = task_current->process;

    task_enter();
}

void syscall_sleep(long delay) {
    if (delay == -1) {
        task_remove(task_current, TASK_QUEUE_RUNNABLE);
        task_remove(task_current, TASK_QUEUE_DEAD);

        task_switch_full();
    }
    task_current->sreg_a = delay / (1000 / CPU_TIMER_HZ);
    task_current->pass = true;

    task_remove(task_current, TASK_QUEUE_RUNNABLE);
    task_insert(task_current, TASK_QUEUE_SLEEPING);

    task_switch_full();
}

void syscall_yield() {
    task_switch_full();
}

void syscall_proctest() {
    printf("syscall boi from userspace\n");
}

void task_init() {
    idle_task = task_create(true, idle_task_loop, cpu_get_kernel_page_dir());
    idle_task->process = process_current;

    task0 = task_create(true, NULL, cpu_get_kernel_page_dir());
    task0->process = process_current;
    task_insert(task0, TASK_QUEUE_RUNNABLE);

    task_current = task0;
    task_current_context = task0->context;

    syscalls[0] = syscall_sleep;
    syscalls[1] = syscall_yield;
    syscalls[2] = syscall_proctest;
}