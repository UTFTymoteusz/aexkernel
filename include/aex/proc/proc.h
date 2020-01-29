#pragma once

#include "aex/kernel.h"
#include "aex/klist.h"
#include "aex/mutex.h"
#include "aex/io.h"

#include "aex/fs/file.h"

#include "mem/pagetrk.h"

#define KERNEL_PROCESS 1
#define INIT_PROCESS   2

// The system will abort if this thread aborts unexpectedly
#define THREAD_FLAG_CRITICAL 0x0001

#define PROC_FLAG_NONE       0x0000 // Fancy
#define PROC_FLAG_MARK_KILL  0x0001 // This flag marks processes that are about to get killed. If this flag is true, the process_get*() will return NULL when trying to get the process.

typedef int64_t pid_t;
typedef int64_t tid_t;

struct hook_proc_data {
    uint64_t pid;
};
typedef struct hook_proc_data hook_proc_data_t;

struct thread {
    tid_t id;

    char* name;

    mutex_t access;

    pid_t parent_pid;
    struct task* task;

    uint64_t rreg0;
    uint64_t rreg1;

    uint16_t flags;
};
typedef struct thread thread_t;

struct process {
    pid_t pid;

    char* name;
    char* image_path;
    char* working_dir;

    struct klist threads;
    struct klist fiddies;

    volatile int busy;
    mutex_t access;

    bqueue_t wait_list;

    uint16_t flags;

    uint64_t thread_counter;
    uint64_t fiddie_counter;

    pid_t parent_pid;

    page_root_t* proot;
};
typedef struct process process_t;

struct process* process_current;
struct klist process_klist;

tid_t thread_create(pid_t pid, void* entry, bool kernelmode);

void thread_start (pid_t pid, tid_t id);
bool thread_kill  (pid_t pid, tid_t id);

thread_t* thread_get(pid_t pid, tid_t id);

bool thread_pause (pid_t pid, tid_t id);
bool thread_resume(pid_t pid, tid_t id);
bool thread_exists(pid_t pid, tid_t id);

pid_t process_create (char* name, char* image_path, size_t paging_dir);
pid_t process_icreate(char* image_path, char* args[]);

int    process_start(pid_t pid);
bool   process_kill (pid_t pid);

process_t* process_get(pid_t pid);

bool process_lock(pid_t pid);
void process_unlock(pid_t pid);

int64_t process_used_phys_memory(pid_t pid);
int64_t process_mapped_memory   (pid_t pid);

void proc_set_stdin (pid_t pid, file_t* fd);
void proc_set_stdout(pid_t pid, file_t* fd);
void proc_set_stderr(pid_t pid, file_t* fd);
void proc_set_dir   (pid_t pid, char* path);

bool process_use(pid_t pid);
void process_release(pid_t pid);

void process_debug_list();