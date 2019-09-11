#pragma once

#include "aex/klist.h"
#include "aex/kmem.h"

#include "dev/cpu.h"
#include "proc/task.h"

struct process {
    size_t pid;

    char* name;
    char* image_path;

    struct klist threads, fiddies;

    size_t thread_counter;
    size_t fiddie_counter;

    size_t paging_dir;
};
struct thread {
    size_t id;

    struct process* parent;
    struct task_descriptor* task;
};

struct klist* process_klist;

size_t process_counter = 1;


struct thread* thread_create(struct process* process, void* entry) {

    struct thread* new_thread = (struct thread*)kmalloc(sizeof(struct thread));

    new_thread->task = task_create(false, entry, process->paging_dir);
    task_insert(new_thread->task, TASK_QUEUE_RUNNABLE);

    new_thread->id = process->thread_counter++;
    new_thread->parent = process;

    klist_set(&process->threads, new_thread->id, new_thread);

    return new_thread;
}
bool thread_kill(struct thread* thread) {

    if (klist_get(&thread->parent->threads, thread->id) == NULL)
        return false;

    kpanic("Unimplemented thread task killing bs");

    kfree((void*)thread);
    klist_set(process_klist, thread->id, NULL);

    return true;
}


size_t process_create(char* name, char* image_path, size_t paging_dir) {
    
    struct process* new_process = (struct process*)kmalloc(sizeof(struct process));

    new_process->pid        = process_counter++;
    new_process->name       = name;
    new_process->image_path = image_path;
    new_process->paging_dir = paging_dir;

    klist_init(&new_process->threads);
    klist_init(&new_process->fiddies);

    new_process->thread_counter = 8;
    new_process->fiddie_counter = 128;

    klist_set(process_klist, new_process->pid, (void*)new_process);

    printf("Created process '%s'\n", name);

    return new_process->pid;
}
struct process* process_get(size_t pid) {
    return (struct process*)klist_get(process_klist, pid);
}
bool process_kill(size_t pid) {

    struct process* process = process_get(pid);

    if (process == NULL)
        return false;

    printf("Killed process '%s'\n", process->name);

    process->name       = NULL;
    process->image_path = NULL;

    struct thread* thread;

    while (process->threads.count) {

        thread = (struct thread*)klist_get(&process->threads, klist_first(&process->threads));
        thread_kill(thread);
    }

    kfree((void*)process);
    klist_set(process_klist, pid, NULL);

    return true;
}

void proc_init() {
    klist_init(process_klist);

    size_t pid = process_create("system", "/sys/aexkrnl.elf", 0);
    process_current = process_get(pid);

	write_debug("system pid: %s\n", process_current->pid, 10);
}