#include "aex/klist.h"
#include "aex/kmem.h"
#include "aex/rcode.h"
#include "aex/time.h"

#include "dev/cpu.h"

#include "proc/task.h"
#include "proc/elfload.h"

#include "fs/fs.h"

#include <stdio.h>
#include <string.h>

#include "proc.h"

struct process;
struct thread;

struct klist process_klist;
size_t process_counter = 1;

struct thread* thread_create(struct process* process, void* entry, bool kernelmode) {
    struct thread* new_thread = kmalloc(sizeof(struct thread));
    memset(new_thread, 0, sizeof(struct thread));

    new_thread->task = task_create(kernelmode, entry, process->paging_dir);
    task_insert(new_thread->task, TASK_QUEUE_RUNNABLE);

    new_thread->id = process->thread_counter++;
    new_thread->parent = process;

    klist_set(&process->threads, new_thread->id, new_thread);

    return new_thread;
}

bool thread_kill(struct thread* thread) {
    if (klist_get(&thread->parent->threads, thread->id) == NULL)
        return false;

    task_remove(thread->task, TASK_QUEUE_RUNNABLE);
    task_remove(thread->task, TASK_QUEUE_SLEEPING);

    kfree(thread);
    klist_set(&thread->parent->threads, thread->id, NULL);

    return true;
}

size_t process_create(char* name, char* image_path, size_t paging_dir) {
    struct process* new_process = kmalloc(sizeof(struct process));
    memset(new_process, 0, sizeof(struct process));

    new_process->pid        = process_counter++;
    new_process->paging_dir = paging_dir;
    new_process->name       = kmalloc(strlen(name) + 2);
    new_process->image_path = kmalloc(strlen(image_path) + 2);

    strcpy(new_process->name,       name);
    strcpy(new_process->image_path, image_path);

    klist_init(&new_process->threads);
    klist_init(&new_process->fiddies);

    new_process->thread_counter = 0;
    new_process->fiddie_counter = 4;

    klist_set(&process_klist, new_process->pid, new_process);

    printf("Created process '%s' with pid %i\n", name, new_process->pid);

    return new_process->pid;
}

struct process* process_get(size_t pid) {
    return klist_get(&process_klist, pid);
}

bool process_kill(size_t pid) {
    struct process* process = process_get(pid);
    if (process == NULL)
        return false;

    printf("Killed process '%s'\n", process->name);

    kfree(process->name);
    kfree(process->image_path);

    struct thread* thread;
    bool self = process->pid == process_current->pid;

    while (process->threads.count) {
        thread = klist_get(&process->threads, klist_first(&process->threads));
        thread_kill(thread);
    }
    kfree(process);
    klist_set(&process_klist, pid, NULL);

    if (self)
        yield();

    return true;
}

int process_icreate(char* image_path) {
    int ret;
    
    if (!fs_fexists(image_path))
        return FS_ERR_NOT_FOUND;

    char* name = image_path + strlen(image_path);
    while (*name != '/')
        --name;

    name++;

    char* max = image_path + strlen(image_path);
    char* end = name;
    while (*end != '.' && end <= max)
        end++;

    ret = elf_load(image_path);
    
    char before = *end;
    if (*end == '.')
        *end = '\0';

    ret  = process_create(name, image_path, 0);
    *end = before;

    struct process* process = process_get(ret);
    thread_create(process, NULL, false);

    return ret;
}

void proc_init() {
    klist_init(&process_klist);

    size_t pid = process_create("system", "/sys/aexkrnl.elf", 0);
    process_current = process_get(pid);
}

void proc_initsys() {
    struct thread* new_thread = kmalloc(sizeof(struct thread));
    memset(new_thread, 0, sizeof(struct thread));

    new_thread->id     = process_current->thread_counter++;
    new_thread->parent = process_current;
    new_thread->name   = "Main Kernel Thread";

    klist_set(&process_current->threads, new_thread->id, new_thread);
    new_thread->task = task_current;
}