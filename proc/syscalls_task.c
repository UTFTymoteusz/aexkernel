#include "aex/rcode.h"
#include "aex/syscall.h"
#include "aex/fs/fs.h"
#include "aex/fs/fd.h"
#include "aex/proc/task.h"
#include <stdbool.h>

extern rcptable(process_t) processes;

void syscall_sleep(int64_t delay) {
    if (delay < 0) {
        task_tset_status(CURRENT_TID, THREAD_STATUS_BLOCKED);
        task_tyield();

        return;
    }
    task_tsleep(delay);
}

void syscall_yield() {
    task_tyield();
}

struct spawn_options_usr {
    int stdin;
    int stdout;
    int stderr;
};
typedef struct spawn_options_usr spawn_options_usr_t;

int64_t syscall_spawn(char* image_path, spawn_options_usr_t* options, char* args[]) {
    process_t* process = rcptable_get(processes, CURRENT_PID);
    if (process == NULL)
        return -EINTR;

    char* _path = fs_to_absolute_path(image_path, process->cwd);

    int stdin  = *(int*) rcparray_get(process->fiddies, 0);
    int stdout = *(int*) rcparray_get(process->fiddies, 1);
    int stderr = *(int*) rcparray_get(process->fiddies, 2);

    rcparray_unref(process->fiddies, 0);
    rcparray_unref(process->fiddies, 1);
    rcparray_unref(process->fiddies, 2);

    if (options != NULL) {
        int* ptr;

        if (options->stdin != 0) {
            ptr = rcparray_get(process->fiddies, options->stdin);
            if (ptr != NULL) {
                stdin = *ptr;
                rcparray_unref(process->fiddies, options->stdin);
            }
        }
        if (options->stdout != 0) {
            ptr = rcparray_get(process->fiddies, options->stdout);
            if (ptr != NULL) {
                stdout = *ptr;
                rcparray_unref(process->fiddies, options->stdout);
            }
        }
        if (options->stderr != 0) {
            ptr = rcparray_get(process->fiddies, options->stderr);
            if (ptr != NULL) {
                stderr = *ptr;
                rcparray_unref(process->fiddies, options->stderr);
            }
        }
    }
    stdin  = fd_dup(stdin);
    stdout = fd_dup(stdout);
    stderr = fd_dup(stderr);

    pcreate_info_t pinfo = {
        .stdin  = stdin,
        .stdout = stdout,
        .stderr = stderr,
        .cwd    = process->cwd,
    };

    pid_t pid = task_pricreate(_path, args, &pinfo);
    rcptable_unref(processes, CURRENT_PID);

    kfree(_path);

    IF_ERROR(pid) {
        fd_close(stdin);
        fd_close(stdout);
        fd_close(stderr);
        
        return pid;
    }
    task_prstart(pid);
    return pid;
}

int64_t syscall_wait(pid_t pid) {
    int code = 0;

    task_prwait(pid, &code);
    return code;
}

void syscall_exit(int code) {
    task_prkill(CURRENT_PID, code);

    while (!task_tshould_exit(CURRENT_TID))
        task_tyield();
}

int64_t syscall_getcwd(char* buffer, size_t len) {
    return task_prget_cwd(CURRENT_PID, buffer, len);
}

int64_t syscall_setcwd(char* buffer) {
    return task_prset_cwd(CURRENT_PID, buffer);
}

void task_syscall_init() {
    syscalls[SYSCALL_SLEEP] = syscall_sleep;
    syscalls[SYSCALL_YIELD] = syscall_yield;
    syscalls[SYSCALL_SPAWN] = syscall_spawn;
    syscalls[SYSCALL_WAIT]  = syscall_wait;
    syscalls[SYSCALL_EXIT]  = syscall_exit;

    syscalls[SYSCALL_GETCWD] = syscall_getcwd;
    syscalls[SYSCALL_SETCWD] = syscall_setcwd;
}