#pragma once

#include "aex/io.h"
#include "aex/kernel.h"
#include "aex/klist.h"
#include "aex/mem.h"
#include "aex/spinlock.h"

#define KERNEL_PROCESS 1
#define INIT_PROCESS   2

// The system will abort if this thread aborts unexpectedly
#define THREAD_FLAG_CRITICAL 0x0001

#define PROC_FLAG_NONE       0x0000 // Fancy

#define PROC_STATUS_HEALTHY 0x00
#define PROC_STATUS_ZOMBIE  0x01

typedef int64_t pid_t;
typedef int64_t tid_t;

struct thread {
    tid_t id;

    char* name;

    spinlock_t access;

    pid_t parent_pid;
    struct task* task;

    uint64_t rreg0;
    uint64_t rreg1;

    uint16_t flags;
};
typedef struct thread thread_t;

struct process {
    pid_t pid;

    uint8_t status;
    int rcode;

    char* name;
    char* image_path;
    char* working_dir;

    struct klist threads;
    struct klist fiddies;

    volatile int busy;
    spinlock_t access;

    bqueue_t wait_list;

    uint16_t flags;

    uint64_t thread_counter;
    uint64_t fiddie_counter;

    pid_t parent_pid;

    pagemap_t* proot;
};
typedef struct process process_t;

struct hook_proc_data {
    pid_t pid;
    process_t* process;
};
typedef struct hook_proc_data hook_proc_data_t;

struct process* process_current;
struct klist process_klist;

tid_t thread_create(pid_t pid, void* entry, bool kernelmode);

void thread_start (pid_t pid, tid_t id);
bool thread_kill  (pid_t pid, tid_t id);

thread_t* thread_get(pid_t pid, tid_t id);

bool thread_pause (pid_t pid, tid_t id);
bool thread_resume(pid_t pid, tid_t id);
bool thread_exists(pid_t pid, tid_t id);

pid_t process_create (char* name, char* image_path, pagemap_t* pmap);
pid_t process_icreate(char* image_path, char* args[]);

int  process_start(pid_t pid);
bool process_kill (pid_t pid);

/*
 * Attempts to get the struct of a process. Returns NULL when a process
 * of that PID doesn't exist or is about to get killed.
 */
process_t* process_get(pid_t pid);
/*
 * Gets the return code of a zombie process and removes it from the process
 * list.
 */
int process_get_rcode(pid_t pid);

/*
 * Locks the process. Return true if process has been locked.
 */
bool process_lock(pid_t pid);
/*
 * Unlocks the process. It should be done only after process_lock()ing
 * the process in question.
 */
void process_unlock(pid_t pid);

int64_t process_used_phys_memory(pid_t pid);
int64_t process_mapped_memory   (pid_t pid);

/*
 * Sets the stdin of a process. Automatically decrements the reference
 * counter of the old stdin and increments the reference counter of
 * the new stdin.
 */
void proc_set_stdin(pid_t pid, int fd);
/*
 * Sets the stdout of a process. Automatically decrements the reference
 * counter of the old stdout and increments the reference counter of
 * the new stdout.
 */
void proc_set_stdout(pid_t pid, int fd);
/*
 * Sets the stderr of a process. Automatically decrements the reference
 * counter of the old stderr and increments the reference counter of
 * the new stderr.
 */
void proc_set_stderr(pid_t pid, int fd);
/*
 * Sets the working directory of the process. This path must be absolute.
 */
void proc_set_dir   (pid_t pid, char* path);

/*
 * Makes sure the process won't get killed. Returns true if the process
 * has been "used".
 */
bool process_use(pid_t pid);
/*
 * Releases the process, allowing it to be killed. It should be done only
 * after process_lock()ing the process in question.
 */
void process_release(pid_t pid);

void process_debug_list();