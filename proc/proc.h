#pragma once

#include "aex/klist.h"

#include "fs/fs.h"

#include "mem/pagetrk.h"

struct thread {
    uint64_t id;

    char* name;

    struct process* process;
    struct task_descriptor* task;
};

struct process {
    uint64_t pid;

    char* name;
    char* image_path;

    struct klist threads, fiddies;

    uint64_t thread_counter;
    uint64_t fiddie_counter;

    //file_t* stdin;  // For easy reference
    //file_t* stdout;
    //file_t* stderr;

    page_tracker_t* ptracker;
};

struct process* process_current;
struct klist process_klist;

void proc_init();
void proc_initsys();

struct thread* thread_create(struct process* process, void* entry, bool kernelmode);
bool   thread_kill(struct thread* thread);

size_t process_create(char* name, char* image_path, size_t paging_dir);
struct process* process_get(size_t pid);
bool   process_kill(size_t pid);
int    process_icreate(char* image_path);

uint64_t process_used_memory(size_t pid);

void process_debug_list();

void proc_set_stdin(struct process* process, file_t* fd);
void proc_set_stdout(struct process* process, file_t* fd);
void proc_set_stderr(struct process* process, file_t* fd);