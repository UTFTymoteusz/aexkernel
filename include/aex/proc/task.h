#pragma once

#include "aex/sys/cpu.h"

#include "aex/event.h"
#include "aex/mem.h"
#include "aex/rcparray.h"
#include "aex/rcptable.h"

#include "cpu_int.h"

#include <stdint.h>

#define CURRENT_PID     task_cpu_locals[CURRENT_CPU].current_pid
#define CURRENT_TID     task_cpu_locals[CURRENT_CPU].current_tid
#define CURRENT_CONTEXT task_cpu_locals[CURRENT_CPU].current_context

#define KERNEL_PROCESS 1
#define INIT_PROCESS   2

enum thread_status {
    THREAD_STATUS_CREATED  = 0,
    THREAD_STATUS_RUNNABLE = 1,
    THREAD_STATUS_SLEEPING = 2,
    THREAD_STATUS_BLOCKED  = 3,
    THREAD_STATUS_DEAD     = 4,
};

typedef int tid_t;
typedef int pid_t;

struct cpu_local {
    int cpu_id;  // 0

    pid_t current_pid; // 4
    tid_t current_tid; // 8
    thread_context_t* current_context; // 16

    tid_t last_tid;     // 24
    void* kernel_stack; // 32
    void* user_stack;   // 40 
    int   reshed_block; // 48

    int should_reshed;  // 52
};
typedef struct cpu_local task_cpu_local_t;


struct process {
    char* name;
    char* cwd;

    pid_t parent;

    volatile int busy;
    pagemap_t* pagemap;

    event_t signal_event;

    rcparray(int)   fiddies;
    rcparray(tid_t) threads;

    spinlock_t fiddie_lock;
    spinlock_t thread_lock;

    int ecode;
};
typedef struct process process_t;

struct process_info {
    size_t used_phys_memory;
    size_t mapped_memory;

    size_t dir_phys_usage;

    pid_t parent;
};
typedef struct process_info process_info_t;

struct pcreate_info {
    int stdin;
    int stdout;
    int stderr;
    char* cwd;
};
typedef struct pcreate_info pcreate_info_t;

struct pkill_hook_data {
    pid_t pid;
    process_t* pstruct;
};
typedef struct pkill_hook_data pkill_hook_data_t;


struct thread {
    pid_t process;

    void* kernel_stack;
    void* saved_stack;
    void* initial_user_stack;
    
    pagemap_t* pagemap;

    uint8_t status;
    bool kernelmode;

    size_t resume_after;
    volatile bool abort;
    volatile bool running;

    volatile int busy;

    thread_context_t context;
};
typedef struct thread thread_t;


task_cpu_local_t* task_cpu_locals;


void task_init();

/*
 * Creates a new thread and calls the specified function asynchronously with 
 * the specified argument passed to it. Calls the specified callback with the 
 * first argument being the value returned by the function if callback is not 
 * NULL. Returns 0 on success or an error code on failure. 
 */
int task_call_async(void* (*func)(void* arg), void* arg, 
                void (*callback)(void* ret_val));

/*
 * Creates a new thread and returns its TID or -1 on failure. The thread will
 * get added to the specified process local thread list.
 */
tid_t task_tcreate(pid_t pid, void* entry, bool kernelmode);

/*
 * Starts a thread.
 */
void task_tstart(tid_t tid);

/*
 * Kills a thread.
 */
void task_tkill(tid_t tid);

/*
 * Puts the currently running thread to sleep for the specified amount in
 * miliseconds.
 */
void task_tsleep(size_t delay);

/*
 * Checks if the currently running thread can be yielded.
 */
bool task_tcan_yield();

/*
 * Yields the currently running threads' CPU time slice.
 */
void task_tyield();

/*
 * Tries to yield the currently running thread. Returns false on failure.
 */
bool task_ttry_yield();

/*
 * Sets the status of a thread.
 */
void task_tset_status(tid_t tid, enum thread_status status);

/*
 * Gets the status of a thread.
 */
enum thread_status task_tget_status(tid_t tid);

/*
 * Sets the arguments of a thread.
 */
void task_tset_args(tid_t tid, int argc, ...);

/*
 * Checks whenever a thread has received the abort signal.
 */
bool task_tshould_exit(tid_t tid);

/*
 * Increments the busy counter of a thread. If a thread is "busy", it's not
 * allowed to get killed and prevents the parent process from getting killed.
 * Returns true if further operation with the thread is allowed.
 */
bool task_tuse(tid_t tid);

/*
 * Decreases the busy counter of a thread. If a thread is "busy", it's not
 * allowed to get killed and prevents the parent process from getting killed.
 */
void task_trelease(tid_t tid);

/*
 * Returns the parent process of a thread.
 */
pid_t task_tprocessof(tid_t tid);


/*
 * Creates a new process and returns its PID or an error code on failure.
 */
pid_t task_prcreate(char* name, char* image_path, 
                pagemap_t* pmap, pcreate_info_t* info);

/*
 * Creates a new process from an image and returns its PID or -1 on 
 * failure.
 */
pid_t task_pricreate(char* image_path, char* args[], pcreate_info_t* info);

/*
 * Starts a process. This means starting every thread it owns.
 */
void task_prstart(pid_t pid);

/*
 * Kills a process.
 */
void task_prkill(pid_t pid, int code);

/*
 * Gets a pointer to a process pagemap.
 */
pagemap_t* task_prget_pmap(pid_t pid);

/*
 * Retrieves information about a process. Fills the pointed-to struct.
 * Returns 0 on success and a negative value on failure.
 */
int task_prinfo(pid_t pid, process_info_t* info);

/*
 * Increments the busy counter of a process. If a process is "busy" it's not
 * allowed to get killed. Threads, however, can still get killed even if a 
 * process is "busy". Look into task_tuse() for usage with threads. 
 * Returns true on success.
 */
bool task_pruse(pid_t pid);

/*
 * Decrements the busy counter of a process. If a process is "busy" it's not
 * allowed to get killed. Threads, however, can still get killed even if a 
 * process is "busy". Look into task_tuse() for usage with threads. 
 */
void task_prrelease(pid_t pid);

/*
 * Waits for a process to exit. Returns 0 on success and an error otherwise.
 */
int task_prwait(pid_t pid, int* code);

/*
 * Sets the working directory of a process. Returns 0 on success and an error
 * code on failure.
 */
int task_prset_cwd(pid_t pid, char* dir);

/*
 * Gets the working directory of a process and fills the specified buffer.
 * Returns the directory path length (not counting the null byte at the end)
 * or an error code on failure.
 */
int task_prget_cwd(pid_t pid, char* buffer, size_t size);

/*
 * Increments the local CPU's reshedule block counter. If the block counter is
 * greater than 0, current CPU is not allowed to reshedule by interrupt.
 */
void task_reshed_disable();

/*
 * Decrements the local CPU's reshedule block counter. If the block counter is
 * greater than 0, current CPU is not allowed to reshedule by interrupt.
 */
void task_reshed_enable();