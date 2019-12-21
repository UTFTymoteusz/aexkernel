#pragma once

#include "aex/klist.h"
#include "aex/mutex.h"
#include "aex/io.h"

#include "aex/fs/file.h"

#include <stdio.h>

#include "mem/pagetrk.h"

#define KERNEL_PROCESS 1
#define INIT_PROCESS   2

struct hook_proc_data {
    uint64_t pid;
};
typedef struct hook_proc_data hook_proc_data_t;

struct thread {
    uint64_t id;

    char* name;
    bool pause;
    bool added_busy;

    struct process* process;
    struct task* task;

    uint64_t rreg0;
    uint64_t rreg1;
};
typedef struct thread thread_t;

struct process {
    uint64_t pid;

    char* name;
    char* image_path;
    char* working_dir;

    struct klist threads;
    struct klist fiddies;

    mutex_t access;
    volatile int busy;

    bqueue_t wait_list;

    uint64_t thread_counter;
    uint64_t fiddie_counter;

    uint64_t parent_pid;

    page_tracker_t* ptracker;
};
typedef struct process process_t;

struct process* process_current;
struct klist process_klist;

thread_t* thread_create(struct process* process, void* entry, bool kernelmode);

void thread_start(struct thread* thread);
bool thread_kill(struct thread* thread);
bool thread_pause(struct thread* thread);
bool thread_resume(struct thread* thread);

size_t process_create(char* name, char* image_path, size_t paging_dir);
int    process_icreate(char* image_path, char* args[]);

int    process_start(struct process* process);
bool   process_kill(uint64_t pid);

process_t* process_get(uint64_t pid);

uint64_t process_used_phys_memory(uint64_t pid);
uint64_t process_mapped_memory(uint64_t pid);

void proc_set_stdin (struct process* process, file_t* fd);
void proc_set_stdout(struct process* process, file_t* fd);
void proc_set_stderr(struct process* process, file_t* fd);
void proc_set_dir   (struct process* process, char* path);

static inline void process_use(struct process* process) {
    mutex_acquire_yield(&(process->access));
    process->busy++;
    task_current->thread->added_busy = true;
    mutex_release(&(process->access));
}

static inline void process_release(struct process* process) {
    mutex_acquire_yield(&(process->access));
    process->busy--;
    task_current->thread->added_busy = false;
    mutex_release(&(process->access));

    if (task_current->thread->pause) {
        printf("pauss\n");
        task_remove(task_current);
    }
}

void process_debug_list();