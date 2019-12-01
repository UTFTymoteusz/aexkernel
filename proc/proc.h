#pragma once

#include "aex/klist.h"
#include "aex/mutex.h"
#include "aex/io.h"

#include "fs/file.h"

#include "mem/pagetrk.h"

#define KERNEL_PROCESS 1

struct hook_proc_data {
    uint64_t pid;
};
typedef struct hook_proc_data hook_proc_data_t;

struct thread {
    uint64_t id;

    char* name;

    struct process* process;
    struct task* task;
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

void proc_init();
void proc_initsys();

thread_t* thread_create(struct process* process, void* entry, bool kernelmode);

void thread_start(struct thread* thread);
bool thread_kill(struct thread* thread);
bool thread_pause(struct thread* thread);
bool thread_resume(struct thread* thread);

size_t     process_create(char* name, char* image_path, size_t paging_dir);
process_t* process_get(size_t pid);

bool   process_kill(size_t pid);
int    process_icreate(char* image_path);
int    process_start(struct process* process);
int    process_destroy(struct process* process);

uint64_t process_used_phys_memory(size_t pid);

void process_debug_list();

void proc_set_stdin(struct process* process, file_t* fd);
void proc_set_stdout(struct process* process, file_t* fd);
void proc_set_stderr(struct process* process, file_t* fd);
void proc_set_dir(struct process* process, char* path);

static inline void process_use(struct process* process) {
    mutex_acquire_yield(&(process->access));
    process->busy++;
    mutex_release(&(process->access));
}

static inline void process_release(struct process* process) {
    mutex_acquire_yield(&(process->access));
    process->busy--;
    mutex_release(&(process->access));
}