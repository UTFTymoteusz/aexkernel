#include "aex/cbuf.h"
#include "aex/hook.h"
#include "aex/io.h"
#include "aex/kernel.h"
#include "aex/klist.h"
#include "aex/mem.h"
#include "aex/spinlock.h"
#include "aex/rcode.h"
#include "aex/string.h"
#include "aex/sys.h"
#include "aex/syscall.h"
#include "aex/time.h"

#include "aex/fs/fs.h"
#include "aex/fs/file.h"

#include "aex/proc/task.h"
#include "aex/proc/exec.h"

#include "aex/sys/cpu.h"

#include "mem/frame.h"
#include "mem/page.h"

#include <stddef.h>

#include "kernel/init.h"
#include "aex/proc/proc.h"

struct klist process_klist;
struct klist process_dying_klist;
size_t process_counter = 1;

cbuf_t process_cleanup_queue;
cbuf_t thread_cleanup_queue;

spinlock_t proc_prklist_access = {
    .val = 0,
    .name = "prklist access",
};

struct pid_thread_pair {
    pid_t pid;
    thread_t* thread;
};

tid_t thread_create(pid_t pid, void* entry, bool kernelmode) {
    spinlock_acquire(&proc_prklist_access);
    process_t* process = process_get(pid);
    spinlock_release(&proc_prklist_access);
    
    if (process == NULL)
        return -1;

    spinlock_acquire(&(process->access));
    struct thread* new_thread = kmalloc(sizeof(struct thread));
    memset(new_thread, 0, sizeof(struct thread));

    new_thread->task = task_create(process, kernelmode, entry, (size_t) process->proot->root_dir);
    new_thread->task->thread = new_thread;

    new_thread->id = process->thread_counter++;
    new_thread->parent_pid = process->pid;

    new_thread->task->process = process;

    klist_set(&(process->threads), new_thread->id, new_thread);
    spinlock_release(&(process->access));

    return new_thread->id;
}

void thread_start(pid_t pid, tid_t id) {
    spinlock_acquire(&proc_prklist_access);
    process_t* process = process_get(pid);
    spinlock_release(&proc_prklist_access);
    
    if (process == NULL)
        return;

    spinlock_acquire(&(process->access));

    thread_t* thread = klist_get(&(process->threads), id);
    if (thread == NULL)
        return;

    task_insert(thread->task);

    spinlock_release(&(process->access));
}

void _thread_kill(pid_t pid, struct thread* thread) {
    spinlock_acquire(&proc_prklist_access);
    process_t* process = process_get(pid);
    spinlock_release(&proc_prklist_access);
    
    if (process == NULL)
        return;

    spinlock_acquire(&(process->access));

    bool inton = checkinterrupts();
    if (inton)
        nointerrupts();

    task_t* task = thread->task;
    task->status = TASK_STATUS_DEAD;

    task->pause = true;
    while (task->busy == 0)
        ;

    if (thread->task->in_queue)
        task_remove(thread->task);

    task_dispose(thread->task);

    klist_set(&(process->threads), thread->id, NULL);

    if (thread->name != NULL)
        kfree(thread->name);

    kfree(thread);

    spinlock_release(&(process->access));

    if (process->threads.count == 0)
        process_kill(process->pid);

    if (inton)
        interrupts();
}

bool thread_kill(pid_t pid, tid_t id) {
    spinlock_acquire(&proc_prklist_access);
    process_t* process = process_get(pid);
    spinlock_release(&proc_prklist_access);

    if (process == NULL)
        return false;

    spinlock_acquire(&(process->access));

    thread_t* thread = klist_get(&(process->threads), id);
    if (thread == NULL)
        return false;

    struct pid_thread_pair ptp = {
        pid    = pid,
        thread = thread,
    };
    if (thread == task_current->thread)
        cbuf_write(&thread_cleanup_queue, (uint8_t*) &ptp, sizeof(struct pid_thread_pair));
    else
        _thread_kill(pid, thread);

    spinlock_release(&(process->access));
    while (thread_exists(pid, id))
        yield();

    return true;
}

// Only use this on already marked-as-killed processes
bool thread_kill_preserve_process_noint(process_t* process, tid_t id) {
    spinlock_acquire(&(process->access));

    thread_t* thread = klist_get(&(process->threads), id);
    if (thread == NULL)
        return false;

    thread->task->status = TASK_STATUS_DEAD;

    if (thread->task->in_queue)
        task_remove(thread->task);

    task_dispose(thread->task);

    klist_set(&(process->threads), thread->id, NULL);
    kfree(thread);

    spinlock_release(&(process->access));
    return true;
}

thread_t* thread_get(pid_t pid, tid_t id) {
    spinlock_acquire(&proc_prklist_access);
    process_t* process = process_get(pid);
    spinlock_release(&proc_prklist_access);

    if (process == NULL)
        return NULL;

    thread_t* thread = klist_get(&(process->threads), id);
    if (thread == NULL)
        return NULL;

    return thread;
}

void _thread_pause(thread_t* thread) {
    task_t* task = thread->task;
    spinlock_acquire(&(task->running));

    task->pause = true;
    if (task->busy == 0)
        task->status = TASK_STATUS_BLOCKED;
        //task_remove(task);
        
    spinlock_release(&(task->running));
}

bool thread_pause(pid_t pid, tid_t id) {
    spinlock_acquire(&proc_prklist_access);
    process_t* process = process_get(pid);
    spinlock_release(&proc_prklist_access);

    if (process == NULL)
        return false;
    
    spinlock_acquire(&(process->access));

    thread_t* thread = klist_get(&(process->threads), id);
    if (thread == NULL) {
        spinlock_release(&(process->access));
        return false;
    }

    task_t* task = thread->task;
    spinlock_acquire(&(task->running));

    task->pause = true;
    if (task->busy == 0)
        task_remove(task);
        
    spinlock_release(&(task->running));
    spinlock_release(&(process->access));

    return true;
}

bool thread_resume(pid_t pid, tid_t id) {
    spinlock_acquire(&proc_prklist_access);
    process_t* process = process_get(pid);
    spinlock_release(&proc_prklist_access);

    if (process == NULL)
        return false;
    
    spinlock_acquire(&(process->access));

    thread_t* thread = klist_get(&(process->threads), id);
    if (thread == NULL)
        return false;

    task_t* task = thread->task;
    spinlock_acquire(&(task->running));

    task->pause = false;

    if (!task->in_queue)
        task_insert(task);

    spinlock_release(&(task->running));
    spinlock_release(&(process->access));

    return true;
}

bool thread_exists(pid_t pid, tid_t id) {
    spinlock_acquire(&proc_prklist_access);
    process_t* process = process_get(pid);
    spinlock_release(&proc_prklist_access);

    if (process == NULL)
        return false;

    spinlock_acquire(&(process->access));

    if (klist_get(&(process->threads), id) == NULL)
        return false;

    spinlock_release(&(process->access));
    return true;
}


pid_t process_create(char* name, char* image_path, size_t paging_dir) {
    process_t* new_process = kmalloc(sizeof(struct process));
    memset(new_process, 0, sizeof(struct process));

    io_create_bqueue(&(new_process->wait_list));

    new_process->proot = kmalloc(sizeof(paging_descriptor_t));
    if (paging_dir != 0)
        kp_init_desc(new_process->proot, paging_dir);

    new_process->pid        = process_counter++;
    new_process->name       = kmalloc(strlen(name) + 2);
    new_process->image_path = kmalloc(strlen(image_path) + 2);
    new_process->parent_pid = process_current != NULL ? process_current->pid : 0;

    strcpy(new_process->name,       name);
    strcpy(new_process->image_path, image_path);

    klist_init(&(new_process->threads));
    klist_init(&(new_process->fiddies));

    new_process->thread_counter = 0;
    new_process->fiddie_counter = 4;

    new_process->access.name = "process access";

    spinlock_acquire(&proc_prklist_access);
    klist_set(&process_klist, new_process->pid, new_process);
    spinlock_release(&proc_prklist_access);

    return new_process->pid;
}

pid_t process_icreate(char* image_path, char* args[]) {
    if (!fs_exists(image_path))
        return ERR_NOT_FOUND;

    char* name = image_path + strlen(image_path);
    while (*name != '/')
        --name;

    name++;

    char* max = image_path + strlen(image_path);
    char* end = name;
    while (*end != '.' && end <= max)
        end++;

    struct exec_data exec;
    CLEANUP paging_descriptor_t* proot = kmalloc(sizeof(paging_descriptor_t));

    int ret = exec_load(image_path, args, &exec, proot);
    RETURN_IF_ERROR(ret);

    char before = *end;
    if (*end == '.')
        *end = '\0';

    pid_t new_pid = process_create(name, image_path, 0);
    *end = before;

    spinlock_acquire(&proc_prklist_access);
    process_t* process = process_get(new_pid);
    spinlock_release(&proc_prklist_access);

    memcpy(process->proot, proot, sizeof(paging_descriptor_t));
    tid_t main_thread_id = thread_create(new_pid, exec.kernel_entry, false);

    if (process_lock(new_pid)) {
        thread_t* main_thread = thread_get(new_pid, main_thread_id);

        main_thread->name = kmalloc(6);
        strcpy(main_thread->name, "Entry");

        init_entry_caller(main_thread->task, exec.entry, exec.init_code_addr, args_count(args));
        process_unlock(new_pid);
    }
    else
        return -1;

    return new_pid;
}

int process_start(pid_t pid) {
    spinlock_acquire(&proc_prklist_access);
    process_t* process = process_get(pid);
    spinlock_release(&proc_prklist_access);

    if (process == NULL)
        return -1;

    klist_entry_t* klist_entry = NULL;
    struct thread* thread = NULL;

    hook_proc_data_t pkill_data = {
        .pid = process->pid,
        .process = process,
    };
    hook_invoke(HOOK_PSTART, &pkill_data);

    while (true) {
        thread = klist_iter(&(process->threads), &klist_entry);
        if (thread == NULL)
            break;

        thread_start(pid, thread->id);
    }
    return 0;
}

void _process_kill(process_t* process) {
    spinlock_wait(&(process->access));

    size_t pid = process->pid;

    thread_t* thread;
    klist_entry_t* klist_entry = NULL;

    while (true) {
        thread = (thread_t*) klist_iter(&(process->threads), &klist_entry);
        if (thread == NULL)
            break;

        _thread_pause(thread);
    }
    while (true) {
        int sum = 0;
        int i = 0;

        klist_entry = NULL;

        while (true) {
            thread = (thread_t*) klist_iter(&(process->threads), &klist_entry);
            if (thread == NULL)
                break;

            if (!task_is_paused(thread->task))
                sum++;

            i++;
        }
        if (sum == 0)
            break;

        yield();
    }
    while (process->busy > 0)
        yield();

    hook_proc_data_t pkill_data = {
        .pid = pid,
        .process = process,
    };
    hook_invoke(HOOK_PKILL, &pkill_data);

    kfree(process->name);
    kfree(process->image_path);
    kfree(process->working_dir);
    
    void* virt;
    phys_addr phys;
    uint32_t flags;

    kpforeach(virt, phys, flags, process->proot) {
        if (!(flags & PAGE_NOPHYS))
            kffree(kfindexof(phys));
    }
    
    while (process->threads.count) {
        thread = klist_get(&(process->threads), klist_first(&process->threads));
        thread_kill_preserve_process_noint(process, thread->id);
    }
    kp_dispose_dir(process->proot->root_dir);

    process->status = PROC_STATUS_ZOMBIE;
    io_defunct(&(process->wait_list));

    kfree(process->proot);
}

bool process_kill(pid_t pid) {
    spinlock_acquire(&proc_prklist_access);
    process_t* process = process_get(pid);
    if (process == NULL) {
        spinlock_release(&proc_prklist_access);
        return false;
    }
    klist_set(&process_klist, pid, NULL); // Make sure no other thread can get this process anymore
    klist_set(&process_dying_klist, pid, process);

    spinlock_release(&proc_prklist_access);

    cbuf_write(&process_cleanup_queue, (uint8_t*) &process, sizeof(process_t*));

    while (klist_get(&process_klist, pid) != NULL || klist_get(&process_dying_klist, pid) != NULL)
        yield();

    return true;
}

process_t* process_get(pid_t pid) {
    return klist_get(&process_klist, pid);
}

int process_get_rcode(pid_t pid) {
    spinlock_acquire(&proc_prklist_access);

    int rcode = ((process_t*) klist_get(&process_dying_klist, pid))->rcode;
    klist_set(&process_dying_klist, pid, NULL);

    spinlock_release(&proc_prklist_access);
    return rcode;
}

bool process_lock(pid_t pid) {
    spinlock_acquire(&proc_prklist_access);
    
    process_t* process = process_get(pid);
    if (process == NULL)
        return false;

    spinlock_release(&proc_prklist_access);

    spinlock_acquire(&(process->access));
    return true;
}

void process_unlock(pid_t pid) {
    spinlock_acquire(&proc_prklist_access);
    process_t* process = process_get(pid);
    spinlock_release(&proc_prklist_access);

    if (process == NULL)
        kpanic("process was somehow null in process_unlock()");

    spinlock_release(&(process->access));
}

void process_debug_list() {
    klist_entry_t* klist_entry = NULL;
    process_t* process = NULL;

    printk("Running processes:\n");
    klist_first(&process_klist);

    while (true) {
        process = klist_iter(&process_klist, &klist_entry);
        if (process == NULL)
            break;

        printk(PRINTK_BARE "[%li] %s\n", process->pid, process->name);

        klist_entry_t* klist_entry2 = NULL;
        struct thread* thread = NULL;

        while (true) {
            thread = klist_iter(&(process->threads), &klist_entry2);
            if (thread == NULL)
                break;

            if (thread->name == NULL)
                printk(PRINTK_BARE " - *anonymous*");
            else
                printk(PRINTK_BARE " - %s", thread->name);

            switch (thread->task->status) {
                case TASK_STATUS_RUNNABLE:
                    printk("; runnable");
                    break;
                case TASK_STATUS_SLEEPING:
                    printk("; sleeping");
                    break;
                case TASK_STATUS_BLOCKED:
                    printk("; blocked");
                    break;
                default:
                    printk("; unknown");
                    break;
            }
            printk("; prio: %i; [%li] 0x%p\n", thread->task->priority, thread->id, thread->task);
        }
    }
}


int64_t process_used_phys_memory(pid_t pid) {
    process_t* process = process_get(pid);
    if (process == NULL)
        return -1;

    return (process->proot->dir_frames_used + process->proot->frames_used - process->proot->map_frames_used) * CPU_PAGE_SIZE;
}

int64_t process_mapped_memory(pid_t pid) {
    process_t* process = process_get(pid);
    if (process == NULL)
        return -1;

    return (process->proot->map_frames_used) * CPU_PAGE_SIZE;
}


static void set_fd(pid_t pid, int fd_id, file_t* file) {
    if (process_lock(pid)) {
        process_t* process = process_get(pid);

        file_t* prev_file = klist_get(&(process->fiddies), fd_id);
        if (prev_file != NULL)
            file_sub_reference(prev_file);

        klist_set(&(process->fiddies), fd_id, file);
        file_add_reference(file);

        process_unlock(pid);
    }
}

void proc_set_stdin(pid_t pid, file_t* fd) {
    set_fd(pid, 0, fd);
}

void proc_set_stdout(pid_t pid, file_t* fd) {
    set_fd(pid, 1, fd);
}

void proc_set_stderr(pid_t pid, file_t* fd) {
    set_fd(pid, 2, fd);
}

void proc_set_dir(pid_t pid, char* path) {
    process_t* process = process_get(pid);
    if (process == NULL)
        return;

    if (process->working_dir != NULL)
        kfree(process->working_dir);

    process->working_dir = kmalloc(strlen(path) + 1);
    strcpy(process->working_dir, path);
}


bool process_use(pid_t pid) {
    spinlock_acquire(&proc_prklist_access);
    process_t* process = process_get(pid);
    spinlock_release(&proc_prklist_access);

    if (process == NULL)
        return false;

    spinlock_acquire(&(process->access));
    process->busy++;
    spinlock_release(&(process->access));

    return true;
}

void process_release(pid_t pid) {
    spinlock_acquire(&proc_prklist_access);
    process_t* process = process_get(pid);
    if (process == NULL)
        process = klist_get(&process_dying_klist, pid);

    spinlock_release(&proc_prklist_access);

    if (process == NULL)
        kpanic("release failed;");

    spinlock_acquire(&(process->access));
    process->busy--;
    spinlock_release(&(process->access));
}

struct spawn_options {
    int stdin;
    int stdout;
    int stderr;
};
typedef struct spawn_options spawn_options_t;

static bool verify_syscall_spawn(spawn_options_t* options) {
    if (options != NULL) {
        if (options->stdin != 0)
            if (klist_get(&(process_current->fiddies), options->stdin) == NULL)
                return false;

        if (options->stdout != 0)
            if (klist_get(&(process_current->fiddies), options->stdout) == NULL)
                return false;

        if (options->stderr != 0)
            if (klist_get(&(process_current->fiddies), options->stderr) == NULL)
                return false;
    }
    return true;
}

int syscall_spawn(char* _image_path, spawn_options_t* options, char* args[]) {
    CLEANUP char* image_path = normalize_user_path(_image_path);

    if (!verify_syscall_spawn(options))
        return ERR_INV_ARGUMENTS;

    task_use(task_current);

    long fret;
    if ((fret = check_user_file_access(image_path, FS_MODE_EXECUTE)) != 0)
        return fret;

    int ret;
    ret = process_icreate(image_path, args);
    IF_ERROR(ret) {
        task_release(task_current);
        return ret;
    }

    file_t* file;

    file = klist_get(&(process_current->fiddies), 0);
    proc_set_stdin(ret, file);

    file = klist_get(&(process_current->fiddies), 1);
    proc_set_stdout(ret, file);

    file = klist_get(&(process_current->fiddies), 2);
    proc_set_stderr(ret, file);

    if (options != NULL) {
        if (options->stdin != 0)
            proc_set_stdin(ret, klist_get(&(process_current->fiddies), options->stdin));

        if (options->stdout != 0)
            proc_set_stdout(ret, klist_get(&(process_current->fiddies), options->stdout));

        if (options->stderr != 0)
            proc_set_stderr(ret, klist_get(&(process_current->fiddies), options->stderr));
    }
    proc_set_dir(ret, process_current->working_dir);

    process_t* proc = process_get(ret);
    proc->parent_pid = process_current->pid;

    process_start(ret);
    task_release(task_current);

    return ret;
}

int syscall_wait(pid_t pid) {
    io_block(&(process_get(pid)->wait_list));
    
    spinlock_acquire(&proc_prklist_access);
    
    process_t* process = klist_get(&process_dying_klist, pid);
    if (process == NULL)
        return 0;

    if (process->parent_pid != process_current->pid)
        return ERR_ACCESS_DENIED;

    spinlock_release(&proc_prklist_access);

    return process_get_rcode(pid);
}

int syscall_getcwd(char* buffer, size_t size) {
    if (strlen(process_current->working_dir) + 1 > size)
        return ERR_NO_SPACE;

    spinlock_acquire(&(process_current->access));
    strcpy(buffer, process_current->working_dir);
    spinlock_release(&(process_current->access));

    return 0;
}

int syscall_setcwd(char* path) {
    finfo_t info;

    int len = strlen(path);

    char* buffer = kmalloc(len + 4);
    translate_path(buffer, NULL, path);

    if (path[len - 1] != '/') {
        buffer[len] = '/';
        buffer[len + 1] = '\0';
    }

    int ret = fs_info(buffer, &info);
    IF_ERROR(ret) {
        kfree(buffer);
        return ret;
    }

    long fret;
    if ((fret = check_user_file_access(buffer, FS_MODE_EXECUTE)) != 0)
        return fret;

    if (info.type != FS_TYPE_DIR) {
        kfree(buffer);
        return ERR_NOT_DIR;
    }
    spinlock_acquire(&(process_current->access));

    kfree(process_current->working_dir);
    process_current->working_dir = buffer;

    spinlock_release(&(process_current->access));

    return 0;
}

void syscall_exit(int code) {
    process_current->rcode = code;

    printk("exit(%li)\n", process_current->pid);
    printk("Process %li (%s) exited with code %i\n", process_current->pid, process_current->name, code);

    process_kill(process_current->pid);
}

bool syscall_thexit() {
    printk("thexit(%li, %li)\n", task_current->thread->parent_pid, task_current->thread->id);
    return thread_kill(task_current->thread->parent_pid, task_current->thread->id);
}

pid_t syscall_getpid() {
    return process_current->pid;
}

tid_t syscall_getthid() {
    return task_current->thread->id;
}

void syscall_proctest() {
    printk("syscall boi from userspace\n");
}

void syscall_sleep(long delay) {
    sleep(delay);
}

void syscall_yield() {
    yield();
}

void proc_init() {
    syscalls[SYSCALL_SPAWN]  = syscall_spawn;
    syscalls[SYSCALL_WAIT]   = syscall_wait;
    syscalls[SYSCALL_GETCWD] = syscall_getcwd;
    syscalls[SYSCALL_SETCWD] = syscall_setcwd;

    syscalls[SYSCALL_SLEEP]   = syscall_sleep;
    syscalls[SYSCALL_YIELD]   = syscall_yield;
    syscalls[SYSCALL_EXIT]    = syscall_exit;
    syscalls[SYSCALL_THEXIT]  = syscall_thexit;
    syscalls[SYSCALL_GETPID]  = syscall_getpid;
    syscalls[SYSCALL_GETTHID] = syscall_getthid;

    syscalls[SYSCALL_PROCTEST] = syscall_proctest;

    klist_init(&process_klist);
    klist_init(&process_dying_klist);

    pid_t pid = process_create("aexkrnl", "/sys/aexkrnl.elf", 0);
    process_current = process_get(pid);

    //kfree(process_current->ptracker); // Look into why this breaks the process klist later
    process_current->proot = &kernel_pgtrk;
}

void process_cleaner() {
    process_t* pr;
    while (true) {
        cbuf_read(&process_cleanup_queue, (uint8_t*) &pr, sizeof(process_t*));
        _process_kill(pr);
    }
}

void thread_cleaner() {
    struct pid_thread_pair ptp;
    while (true) {
        cbuf_read(&thread_cleanup_queue, (uint8_t*) &ptp, sizeof(struct pid_thread_pair));
        _thread_kill(ptp.pid, ptp.thread);
    }
}

void proc_initsys() {
    struct thread* new_thread = kmalloc(sizeof(struct thread));
    memset(new_thread, 0, sizeof(struct thread));

    new_thread->id         = process_current->thread_counter++;
    new_thread->parent_pid = KERNEL_PROCESS;
    new_thread->name       = "Main Kernel Thread";
    new_thread->task       = task_current;

    task_current->thread = new_thread;

    klist_set(&(process_current->threads), new_thread->id, new_thread);
    new_thread->task = task_current;

    cbuf_create(&thread_cleanup_queue, 4096);
    cbuf_create(&process_cleanup_queue, 4096);

    tid_t th_pr_id = thread_create(KERNEL_PROCESS, process_cleaner, true);
    tid_t th_th_id = thread_create(KERNEL_PROCESS, thread_cleaner, true);

    if (process_lock(KERNEL_PROCESS)) {
        thread_t* th_pr = thread_get(KERNEL_PROCESS, th_pr_id);
        th_pr->name = "Process Cleaner";

        thread_t* th_th = thread_get(KERNEL_PROCESS, th_th_id);
        th_th->name = "Thread Cleaner";
        
        process_unlock(KERNEL_PROCESS);
    }
    thread_start(KERNEL_PROCESS, th_pr_id);
    thread_start(KERNEL_PROCESS, th_th_id);
}