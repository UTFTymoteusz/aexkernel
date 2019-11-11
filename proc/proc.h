#pragma once

#include "aex/klist.h"
#include "aex/mutex.h"

#include "fs/fs.h"

#include "mem/pagetrk.h"

#define KERNEL_PROCESS 1

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

    struct klist threads;
    struct klist fiddies;

    mutex_t access;
    int memory_busy;

    uint64_t thread_counter;
    uint64_t fiddie_counter;

    uint64_t parent_pid;

    page_tracker_t* ptracker;
};

struct process* process_current;
struct klist process_klist;

void proc_init();
void proc_initsys();

struct thread* thread_create(struct process* process, void* entry, bool kernelmode);
void thread_start(struct thread* thread);
bool thread_kill(struct thread* thread);
bool thread_pause(struct thread* thread);
bool thread_resume(struct thread* thread);

size_t process_create(char* name, char* image_path, size_t paging_dir);
struct process* process_get(size_t pid);
bool   process_kill(size_t pid);
int    process_icreate(char* image_path);
int    process_start(struct process* process);
int    process_destroy(struct process* process);

uint64_t process_used_phys_memory(size_t pid);

void process_debug_list();

void proc_set_stdin(struct process* process, file_t* fd);
void proc_set_stdout(struct process* process, file_t* fd);
void proc_set_stderr(struct process* process, file_t* fd);