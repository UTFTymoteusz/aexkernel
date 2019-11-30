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

struct task_queue {
    task_t* list_first;
    task_t* current;

    size_t active_count, count;
    size_t shed_counter;
    size_t share;

    uint8_t priority;
};
typedef struct task_queue task_queue_t;

task_t* idle_task;

task_queue_t task_queue_critical;
task_queue_t task_queues[3];

size_t next_id = 1;
volatile size_t task_ticks = 0;

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
    new_task->status   = TASK_STATUS_RUNNABLE;
    new_task->priority = PRIORITY_NORMAL;

    new_task->process = process;

    return new_task;
}

void task_dispose(task_t* task) {
    mempg_free(mempg_indexof(task->kernel_stack - KERNEL_STACK_SIZE, NULL), mempg_to_pages(KERNEL_STACK_SIZE), NULL);
    
    kfree(task->context);
    kfree(task);
}

void _task_insert(task_queue_t* queue, task_t* task) {
    queue->count++;
    if (task->status != TASK_STATUS_SLEEPING)
        queue->active_count++;

    if (queue->list_first == NULL) {
        task->next = NULL;
        task->prev = NULL;

        queue->list_first = task;
        queue->current    = task;
        return;
    }
    task->next = queue->list_first;
    queue->list_first = task;

    (task->next)->prev = task;
}

void task_insert(task_t* task) {
    if (task->in_queue)
        kpanic("Attempt to insert an already-inserted task");

    bool inton = checkinterrupts();

    //mutex_acquire(&(task->access));
    if (inton)
        nointerrupts();

    task->in_queue = true;

    if (task->priority == PRIORITY_CRITICAL)
        _task_insert(&task_queue_critical, task);
    else if (task->priority - PRIORITY_HIGH < TASK_QUEUE_COUNT)
        _task_insert(&(task_queues[task->priority - PRIORITY_HIGH]), task);
    else
        kpanic("Attempt to insert a task with invalid priority");

    //mutex_release(&(task->access));
    if (inton)
        interrupts();
}

void _task_remove(task_queue_t* queue, task_t* task) {
    if (queue->list_first == NULL)
        return;

    if (queue->list_first == task) {
        queue->list_first = queue->list_first->next;

        if (task->status != TASK_STATUS_SLEEPING)
            queue->active_count--;

        queue->count--;
        return;
    }
    if (task->next != NULL)
        (task->next)->prev = task->prev;
        
    (task->prev)->next = task->next;

    if (task->status != TASK_STATUS_SLEEPING)
        queue->active_count--;

    queue->count--;
}

void task_remove(task_t* task) {
    if (!(task->in_queue))
        kpanic("Attempt to remove an already-removed task");

    bool inton = checkinterrupts();

    //mutex_acquire(&(task->access));
    if (inton)
        nointerrupts();

    task->in_queue = false;

    if (task->priority == PRIORITY_CRITICAL)
        _task_remove(&task_queue_critical, task);
    else if (task->priority - PRIORITY_HIGH < TASK_QUEUE_COUNT)
        _task_remove(&(task_queues[task->priority - PRIORITY_HIGH]), task);
    else
        kpanic("Attempt to remove a task with invalid priority");
        
    //mutex_release(&(task->access));
    if (inton)
        interrupts();
}

void task_set_priority(task_t* task, uint8_t priority) {
    nointerrupts();
    bool was_in_queue = task->in_queue;

    if (was_in_queue)
        task_remove(task);

    task->priority = priority;

    if (was_in_queue)
        task_insert(task);
    
    interrupts();
}

void task_put_to_sleep(task_t* task, size_t delay) {
    task->status = TASK_STATUS_SLEEPING;
    task->resume_after = task_ticks + (delay / (1000 / CPU_TIMER_HZ));
    
    if (task->priority - PRIORITY_HIGH < TASK_QUEUE_COUNT && task->priority != PRIORITY_CRITICAL)
        task_queues[task->priority - PRIORITY_HIGH].active_count--;
}

void task_debug() {
    task_queue_t* queue;

    for (int i = 0; i < TASK_QUEUE_COUNT; i++) {
        queue = &(task_queues[i]);
        printf("Queue %i: %iA - %iT\n", i, queue->active_count, queue->count);
    }
}

void task_timer_tick() {
    ++task_ticks;
}

void task_switch_stage2() {
    static int current_queue = 0;

    task_t* task;
    size_t  iter;
    
    size_t ticks = task_ticks;
    size_t total_nc_tasks = 0;

    // Critical tasks need to be scheduled above anything else (duh)
    if (task_queue_critical.count > 0) {
        task = task_queue_critical.current;
        iter = 0;

        while (true) {
            task = task->next;

            if (iter++ > task_queue_critical.count)
                break;

            if (task == NULL)
                task = task_queue_critical.list_first;

            switch (task->status) {
                case TASK_STATUS_SLEEPING:
                    if (ticks >= task->resume_after) {
                        task->status = TASK_STATUS_RUNNABLE;
                        task_queue_critical.current = task;
                        goto task_select_end;
                    }
                    break;
                case TASK_STATUS_BLOCKED:
                    break;
                default:
                    task_queue_critical.current = task;
                    goto task_select_end;
            }
        }
    }

    task_queue_t* queue;

    // Here we count total active tasks. If there are no active tasks, we count total tasks.
    for (int i = 0; i < TASK_QUEUE_COUNT; i++)
        total_nc_tasks += task_queues[i].active_count;

    if (total_nc_tasks == 0) 
        for (int i = 0; i < TASK_QUEUE_COUNT; i++)
            total_nc_tasks += task_queues[i].count;

    // Here we calculate the total time shares for each non-critical queue
    for (int i = 0; i < TASK_QUEUE_COUNT; i++) {
        queue = &(task_queues[i]);
        queue->share = (queue->active_count * queue->priority * 100000) / total_nc_tasks;
    }

    // The reason this is a <= instead of < is so we wrap back around to the initial queue
    for (int i = 0; i <= TASK_QUEUE_COUNT; i++) {
        queue = &(task_queues[current_queue]);

        // If this queue used it's share or there are no tasks on it, move on
        if (queue->shed_counter > queue->share || queue->count == 0) {
            queue->shed_counter = 0;
            current_queue = (current_queue + 1) % TASK_QUEUE_COUNT;

            continue;
        }
        queue->shed_counter += 7500;

        task = queue->current;
        iter = 0;

        while (true) {
            task = task->next;

            if (iter++ > queue->count)
                break;
                
            if (task == NULL)
                task = queue->list_first;

            switch (task->status) {
                case TASK_STATUS_SLEEPING:
                    if (ticks >= task->resume_after) {
                        task->status = TASK_STATUS_RUNNABLE;
                        queue->active_count++;
                        queue->current = task;
                        goto task_select_end;
                    }
                    break;
                case TASK_STATUS_BLOCKED:
                    break;
                default:
                    queue->current = task;
                    goto task_select_end;
            }
        }
        current_queue = (current_queue + 1) % TASK_QUEUE_COUNT;
    }
    task = idle_task;
    
task_select_end:
    task_current = task;
    task_current_context = task->context;

    process_current = task->process;

    task_enter();
}

void syscall_sleep(long delay) {
    if (delay == -1) {
        task_remove(task_current);
        task_switch_full();
    }
    mutex_acquire(&(task_current->access));
    nointerrupts();

    task_put_to_sleep(task_current, delay);

    mutex_release(&(task_current->access));
    interrupts();

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
    memset(&task_queue_critical, 0, sizeof(task_queue_critical));
    memset(task_queues, 0, sizeof(task_queues));
    
    task_queues[0].priority = 4;
    task_queues[1].priority = 2;
    task_queues[2].priority = 1;

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