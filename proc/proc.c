#include "aex/klist.h"
#include "aex/kmem.h"
#include "aex/rcode.h"
#include "aex/time.h"

#include "dev/cpu.h"

#include "proc/task.h"
#include "proc/elfload.h"
#include "proc/exec.h"

#include "fs/fs.h"

#include "mem/frame.h"
#include "mem/page.h"
#include "mem/pagetrk.h"

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

    new_thread->task = task_create(process, kernelmode, entry, (size_t) process->ptracker->root);
    new_thread->task->thread = new_thread;

    new_thread->id = process->thread_counter++;
    new_thread->process = process;

    new_thread->task->process = process;

    klist_set(&process->threads, new_thread->id, new_thread);

    return new_thread;
}

void thread_start(struct thread* thread) {
    task_insert(thread->task);
}

bool thread_kill(struct thread* thread) {
    if (klist_get(&thread->process->threads, thread->id) == NULL)
        return false;

    nointerrupts();

    task_remove(thread->task);
    task_dispose(thread->task);

    kfree(thread);
    klist_set(&thread->process->threads, thread->id, NULL);

    if ((&(thread->process->threads))->count == 0)
        process_kill(thread->process->pid);

    interrupts();
    
    return true;
}

bool thread_pause(struct thread* thread) {
    if (klist_get(&thread->process->threads, thread->id) == NULL)
        return false;

    task_remove(thread->task);
    return true;
}

bool thread_resume(struct thread* thread) {
    if (klist_get(&thread->process->threads, thread->id) == NULL)
        return false;

    task_insert(thread->task);
    return true;
}

size_t process_create(char* name, char* image_path, size_t paging_dir) {
    struct process* new_process = kmalloc(sizeof(struct process));
    memset(new_process, 0, sizeof(struct process));

    new_process->ptracker = kmalloc(sizeof(page_tracker_t));
    if (paging_dir != 0)
        mempg_init_tracker(new_process->ptracker, (void*) paging_dir);

    new_process->pid        = process_counter++;
    new_process->name       = kmalloc(strlen(name) + 2);
    new_process->image_path = kmalloc(strlen(image_path) + 2);

    strcpy(new_process->name,       name);
    strcpy(new_process->image_path, image_path);

    klist_init(&new_process->threads);
    klist_init(&new_process->fiddies);

    new_process->thread_counter = 0;
    new_process->fiddie_counter = 4;

    klist_set(&process_klist, new_process->pid, new_process);

    //printf("Created process '%s' with pid %i\n", name, new_process->pid);

    return new_process->pid;
}

struct process* process_get(size_t pid) {
    return klist_get(&process_klist, pid);
}

void process_debug_list() {
    klist_entry_t* klist_entry = NULL;
    struct process* process = NULL;

    printf("Running processes:\n", klist_first(&process_klist));

    while (true) {
        process = klist_iter(&process_klist, &klist_entry);
        if (process == NULL)
            break;

        printf("[%i] %s\n", process->pid, process->name);
            
        klist_entry_t* klist_entry2 = NULL;
        struct thread* thread = NULL;
            
        while (true) {
            thread = klist_iter(&process->threads, &klist_entry2);
            if (thread == NULL)
                break;

            if (thread->name == NULL)
                printf(" - *anonymous*");
            else
                printf(" - %s", thread->name);

            switch (thread->task->status) {
                case TASK_STATUS_RUNNABLE:
                    printf("; runnable");
                    break;
                case TASK_STATUS_SLEEPING:
                    printf("; sleeping");
                    break;
                case TASK_STATUS_BLOCKED:
                    printf("; blocked");
                    break;
                default:
                    printf("; unknown");
                    break;
            }
            //printf(" @ 0x%x\n", thread->task->context->rip & 0xFFFFFFFFFFFF);
            printf("\n");
        }
    }
}

bool process_kill(size_t pid) {
    struct process* process = process_get(pid);
    if (process == NULL)
        return false;

    thread_t* thread;
    if (process->memory_busy > 0) {
        klist_entry_t* klist_entry = NULL;

        while (true) {
            thread = (thread_t*)klist_iter(&process->threads, &klist_entry);
            if (thread == NULL)
                break;

            thread_pause(thread);
        }
        while (process->memory_busy > 0)
            yield();
    }

    nointerrupts();
    printf("Killed process '%s'\n", process->name);

    kfree(process->name);
    kfree(process->image_path);

    bool self = process->pid == process_current->pid;
    if (self)
        mempg_set_pagedir(NULL);

    page_frame_ptrs_t* ptrs;
    ptrs = &(process->ptracker->first);
    while (ptrs != NULL) {
        for (size_t i = 0; i < PG_FRAME_POINTERS_PER_PIECE; i++) {
            if (ptrs->pointers[i] == 0 || ptrs->pointers[i] == 0xFFFFFFFF)
                continue;

            memfr_unalloc(ptrs->pointers[i]);
        }
        ptrs = ptrs->next;
    }

    ptrs = &(process->ptracker->dir_first);
    while (ptrs != NULL) {
        for (size_t i = 0; i < PG_FRAME_POINTERS_PER_PIECE; i++) {
            if (ptrs->pointers[i] == 0)
                continue;

            memfr_unalloc(ptrs->pointers[i]);
        }
        ptrs = ptrs->next;
    }

    // copy over the stack later on to prevent issues
    while (process->threads.count) {
        thread = klist_get(&process->threads, klist_first(&process->threads));
        thread_kill(thread);
    }
    mempg_dispose_user_root(process->ptracker->root_virt);

    kfree(process->ptracker);
    kfree(process);
    klist_set(&process_klist, pid, NULL);

    interrupts();

    if (self)
        yield();

    return true;
}

int process_icreate(char* image_path) {
    int ret;
    
    if (!fs_exists(image_path))
        return FS_ERR_NOT_FOUND;

    char* name = image_path + strlen(image_path);
    while (*name != '/')
        --name;

    name++;

    char* max = image_path + strlen(image_path);
    char* end = name;
    while (*end != '.' && end <= max)
        end++;

    struct exec_data exec;
    page_tracker_t* tracker = kmalloc(sizeof(page_tracker_t));

    ret = elf_load(image_path, &exec, tracker);
    if (ret < 0) {
        kfree(tracker);
        return ret;
    }
    char before = *end;
    if (*end == '.')
        *end = '\0';

    ret  = process_create(name, image_path, 0);
    *end = before;

    struct process* process = process_get(ret);

    memcpy(process->ptracker, tracker, sizeof(page_tracker_t));
    thread_t* main_thread = thread_create(process, exec.pentry, false);
    main_thread->name = "Entry";

    init_entry_caller(main_thread->task, exec.entry);

    kfree(tracker);
    return ret;
}

int process_start(struct process* process) {
    klist_entry_t* klist_entry = NULL;
    struct thread* thread = NULL;

    while (true) {
        thread = klist_iter(&process->threads, &klist_entry);
        if (thread == NULL)
            break;

        thread_start(thread);
    }
    return 0;
}

uint64_t process_used_phys_memory(size_t pid) {
    struct process* proc = process_get(pid);
    return (proc->ptracker->dir_frames_used + proc->ptracker->frames_used - proc->ptracker->map_frames_used) * CPU_PAGE_SIZE;
}

uint64_t process_mapped_memory(size_t pid) {
    struct process* proc = process_get(pid);
    return (proc->ptracker->map_frames_used) * CPU_PAGE_SIZE;
}

void proc_init() {
    klist_init(&process_klist);

    size_t pid = process_create("aexkrnl", "/sys/aexkrnl.elf", 0);
    process_current = process_get(pid);

    //kfree(process_current->ptracker); // Look into why this breaks the process klist later
    process_current->ptracker = &kernel_pgtrk;
}

void proc_initsys() {
    struct thread* new_thread = kmalloc(sizeof(struct thread));
    memset(new_thread, 0, sizeof(struct thread));

    new_thread->id      = process_current->thread_counter++;
    new_thread->process = process_current;
    new_thread->name    = "Main Kernel Thread";
    new_thread->task    = task_current;
    
    task_current->thread = new_thread;

    klist_set(&process_current->threads, new_thread->id, new_thread);
    new_thread->task = task_current;
}

void proc_set_stdin(struct process* process, file_t* fd) {
    klist_set(&process->fiddies, 0, fd);
}

void proc_set_stdout(struct process* process, file_t* fd) {
    klist_set(&process->fiddies, 1, fd);
}

void proc_set_stderr(struct process* process, file_t* fd) {
    klist_set(&process->fiddies, 2, fd);
}