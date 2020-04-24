#include "aex/cbuf.h"
#include "aex/hook.h"
#include "aex/math.h"
#include "aex/mem.h"
#include "aex/rcode.h"
#include "aex/rcparray.h"
#include "aex/rcptable.h"
#include "aex/spinlock.h"
#include "aex/string.h"

#include "aex/fs/fs.h"
#include "aex/sys/time.h"

#include <stdarg.h>

#include "aex/proc/exec.h"
#include "aex/proc/task.h"

#define USR_STACK_SIZE    8192
#define KERNEL_STACK_SIZE 8192

#define CURRENT_CPU_LOCAL task_cpu_locals[CURRENT_CPU]

rcptable(process_t) processes = {0};
thread_t** threads;

int thread_array_size = 0;

int process_count;
int thread_count;

thread_t* idle_threads;

spinlock_t sheduler_lock = {0};
bool sheduler_ready = false;

task_cpu_local_t* task_cpu_locals = NULL;

const int thread_size = sizeof(thread_t);
const int task_cpu_local_size = sizeof(task_cpu_local_t);

void task_reshed();

static int cwd_isvalid(char* dir) {
    int len = strlen(dir);
    if (len > PATH_LEN_MAX)
        return -ENAMETOOLONG;

    finfo_t finfo;
    int ret = fs_finfo(dir, &finfo);
    RETURN_IF_ERROR(ret);

    if (finfo.type != FS_TYPE_DIR)
        return -ENOTDIR;

    return 0;
}

static tid_t thread_add(thread_t* thread) {
    tid_t tid;

    for (tid = 0; tid < thread_array_size; tid++) {
        if (threads[tid] == NULL) {
            threads[tid] = thread;
            return tid;
        }
    }
    thread_array_size++;

    threads = krealloc(threads, sizeof(thread_t*) * thread_array_size);
    threads[tid] = thread;

    return tid;
}

static thread_t* thread_get(tid_t tid) {
    if (tid >= thread_array_size)
        return NULL;

    return threads[tid];
}

static void thread_remove(tid_t tid) {
    if (tid >= thread_array_size)
        return;
        
    threads[tid] = NULL;
}

static bool thread_try_take(thread_t* thread) {
    return __sync_bool_compare_and_swap(&thread->running, false, true);
}

static void thread_release(thread_t* thread) {
    __sync_bool_compare_and_swap(&thread->running, true, false);
}

void task_set_kernel_stack(thread_t* thread);


static void _call_async(void* (*func)(void* arg), void* arg, 
                void (*callback)(void* ret_val)) {
    void* ret = func(arg);
    
    if (callback != NULL)
        callback(ret);

    task_tkill(CURRENT_TID);
}

int task_call_async(void* (*func)(void* arg), void* arg, 
                void (*callback)(void* ret_val)) {

    tid_t async_thread = task_tcreate(KERNEL_PROCESS, _call_async, true);
    RETURN_IF_ERROR(async_thread);

    task_tset_args(async_thread, 3, func, arg, callback);
    task_tstart(async_thread);

    return 0;
}


tid_t task_tcreate(pid_t pid, void* entry, bool kernelmode) {
    process_t* process = rcptable_get(processes, pid);
    if (process == NULL)
        return -1;

    spinlock_acquire(&sheduler_lock);

    thread_t* thread = kzmalloc(sizeof(thread_t));
    thread->status = THREAD_STATUS_CREATED;
    thread->kernelmode   = kernelmode;
    thread->process      = pid;
    thread->pagemap      = process->pagemap;
    
    void* ker_stack = NULL;
    void* usr_stack = NULL;
    
    if (!kernelmode)
        ker_stack = kpalloc(kptopg(KERNEL_STACK_SIZE), NULL, PAGE_WRITE);

    usr_stack = kpalloc(kptopg(USR_STACK_SIZE), process->pagemap, 
                        kernelmode ? PAGE_WRITE : (PAGE_USER | PAGE_WRITE));

    cpu_fill_context(&thread->context, kernelmode, entry, 
                process->pagemap->root_dir);

    cpu_set_stacks(thread, ker_stack, KERNEL_STACK_SIZE,
                usr_stack, USR_STACK_SIZE);

    thread_count++;

    tid_t tid = thread_add(thread);

    spinlock_release(&sheduler_lock);

    rcparray_add(process->threads, tid);
    rcptable_unref(processes, pid);

    return tid;
}

void task_tstart(tid_t tid) {
    spinlock_acquire(&sheduler_lock);

    thread_t* thread = thread_get(tid);
    if (thread == NULL) {
        spinlock_release(&sheduler_lock);
        return;
    }
    thread->status = THREAD_STATUS_RUNNABLE;

    spinlock_release(&sheduler_lock);
}

void _task_prepare_tkill(tid_t tid) {
    spinlock_acquire(&sheduler_lock);

    thread_t* thread = thread_get(tid);
    if (thread == NULL) {
        spinlock_release(&sheduler_lock);
        return;
    }
    
    __sync_bool_compare_and_swap(&thread->abort, false, true);
    if (thread->status != THREAD_STATUS_DEAD)
        thread->status = THREAD_STATUS_RUNNABLE;
    
    spinlock_release(&sheduler_lock);
}

static void _task_cleanup(tid_t tid, thread_t* thread) {
    spinlock_acquire(&sheduler_lock);
    thread_remove(tid);
    spinlock_release(&sheduler_lock);

    cpu_dispose_stacks(thread, KERNEL_STACK_SIZE, USR_STACK_SIZE);
    kfree(thread);
}

struct thkill_data {
    tid_t tid;
    tid_t requester;
    volatile bool done;
};
typedef struct thkill_data thkill_data_t;

tid_t thread_cleaner_tid = 0;
thkill_data_t* thkill_data = NULL;

void* _task_cleaner(UNUSED void* arg) {
    thread_cleaner_tid = CURRENT_TID;

    thkill_data_t* thdata;

    while (true) {
        thdata = __sync_fetch_and_add(&thkill_data, 0);
        if (thdata == NULL) {
            task_tset_status(CURRENT_TID, THREAD_STATUS_BLOCKED);
            task_tyield();

            continue;
        }

        spinlock_acquire(&sheduler_lock);
        thread_t* thread = thread_get(thdata->tid);
        spinlock_release(&sheduler_lock);

        bool suicide = thdata->tid == thdata->requester;
        if (suicide) {
            task_tset_status(thdata->tid, THREAD_STATUS_DEAD);
            while (!thread_try_take(thread))
                ;
        }

        if (thread != NULL)
            _task_cleanup(thdata->tid, thread);
            
        if (!suicide)
            thdata->done = true;

        thdata = __sync_val_compare_and_swap(&thkill_data, thdata, NULL);
    }
}

void task_tkill(tid_t tid) {
    static thread_context_t a_context;

    spinlock_acquire(&sheduler_lock);

    thread_t* thread = thread_get(tid);
    if (thread == NULL) {
        spinlock_release(&sheduler_lock);
        return;
    }

    __sync_bool_compare_and_swap(&thread->abort, false, true);

    if (thread->status != THREAD_STATUS_DEAD)
        thread->status = THREAD_STATUS_RUNNABLE;

    if (tid != CURRENT_TID) {
        spinlock_release(&sheduler_lock);

        while (__sync_fetch_and_add(&thread->busy, 0) > 0)
            task_tyield();

        task_tset_status(tid, THREAD_STATUS_DEAD);
        while (!thread_try_take(thread))
            ;

        spinlock_acquire(&sheduler_lock);
    }

    // remove thread from process rcparray pls

    pid_t pid = thread->process;
    process_t* process = rcptable_get(processes, pid);
    if (process != NULL) {
        int id;
        rcparray_foreach(process->threads, id) {
            int* lid = rcparray_get(process->threads, id);
            if (lid == NULL)
                continue;

            if (*lid == tid)
                rcparray_remove(process->threads, id);

            rcparray_unref(process->threads, id);
        }
    }

    thkill_data_t data = {
        .tid = tid,
        .requester = CURRENT_TID,
        .done = false,
    };
    spinlock_release(&sheduler_lock);

    while (true) {
        void* prev = __sync_val_compare_and_swap(&thkill_data, NULL, &data);
        task_tset_status(thread_cleaner_tid, THREAD_STATUS_RUNNABLE);

        if (prev == NULL)
            break;
            
        task_tyield();
    }

    if (tid == CURRENT_TID)
        CURRENT_CPU_LOCAL.current_context = &a_context;

    while (tid == CURRENT_TID)
        task_tyield();

    while (!data.done)
        task_tyield();
}

void task_tsleep(size_t delay) {
    spinlock_acquire(&sheduler_lock);

    thread_t* thread = thread_get(CURRENT_TID);
    if (thread == NULL) {
        spinlock_release(&sheduler_lock);
        return;
    }
    thread->status       = THREAD_STATUS_SLEEPING;
    thread->resume_after = time_get_ms_passed() + delay;

    spinlock_release(&sheduler_lock);
    task_reshed();
}

bool task_tcan_yield() {
    return __sync_add_and_fetch(&CURRENT_CPU_LOCAL.reshed_block, 0) == 0;      
}

void task_tyield() {
    if (__sync_add_and_fetch(&CURRENT_CPU_LOCAL.reshed_block, 0) > 0)
        kpanic("task_tyield() while scheduling is disabled");

    task_reshed();
}

bool task_ttry_yield() {
    if (!task_tcan_yield())
        return false;

    task_reshed();
    return true;
}

void task_tset_status(tid_t tid, enum thread_status status) {
    spinlock_acquire(&sheduler_lock);

    thread_t* thread = thread_get(tid);
    if (thread == NULL) {
        spinlock_release(&sheduler_lock);
        return;
    }
    thread->status = status;

    spinlock_release(&sheduler_lock);
}

enum thread_status task_tget_status(tid_t tid) {
    spinlock_acquire(&sheduler_lock);

    thread_t* thread = thread_get(tid);
    if (thread == NULL) {
        spinlock_release(&sheduler_lock);
        return THREAD_STATUS_DEAD;
    }
    uint8_t ret = thread->status;

    spinlock_release(&sheduler_lock);
    return ret;
}

void task_tset_args(tid_t tid, int argc, ...) {
    spinlock_acquire(&sheduler_lock);
    
    thread_t* thread = thread_get(tid);
    if (thread == NULL) {
        spinlock_release(&sheduler_lock);
        return;
    }
    va_list args;
    va_start(args, argc);    

    set_varguments(thread, argc, args);

    va_end(args);

    spinlock_release(&sheduler_lock);
}

bool task_tshould_exit(tid_t tid) {
    spinlock_acquire(&sheduler_lock);

    thread_t* thread = thread_get(tid);
    if (thread == NULL) {
        spinlock_release(&sheduler_lock);
        return true;
    }
    bool ret = thread->abort > 0;

    spinlock_release(&sheduler_lock);
    return ret;
}

bool task_tuse(tid_t tid) {
    spinlock_acquire(&sheduler_lock);

    thread_t* thread = thread_get(tid);
    if (thread == NULL) {
        spinlock_release(&sheduler_lock);
        return false;
    }
    __sync_add_and_fetch(&thread->busy, 1);

    spinlock_release(&sheduler_lock);
    return true;
}

void task_trelease(tid_t tid) {
    spinlock_acquire(&sheduler_lock);

    thread_t* thread = thread_get(tid);
    if (thread == NULL) {
        spinlock_release(&sheduler_lock);
        return;
    }
    int busy = __sync_sub_and_fetch(&thread->busy, 1);
    bool abort = thread->abort > 0;
    spinlock_release(&sheduler_lock);

    if (CURRENT_TID == tid && abort && busy == 0) {
        task_tset_status(CURRENT_TID, THREAD_STATUS_BLOCKED);
        task_tyield();
    }
}

pid_t task_tprocessof(tid_t tid) {
    spinlock_acquire(&sheduler_lock);

    thread_t* thread = thread_get(tid);
    if (thread == NULL) {
        spinlock_release(&sheduler_lock);
        return -EINVAL;
    }
    pid_t ret = thread->process;

    spinlock_release(&sheduler_lock);
    return ret;
}


pid_t task_prcreate(char* name, UNUSED char* image_path, 
                pagemap_t* pmap, pcreate_info_t* pinfo) {
    if (pinfo != NULL) {
        if (pinfo->cwd != NULL)
            RETURN_IF_ERROR(cwd_isvalid(pinfo->cwd));
    }

    process_t process = {0};
    process.pagemap = pmap;
    process.name    = kmalloc(strlen(name) + 1);

    if (task_cpu_locals != NULL)
        process.parent  = CURRENT_PID;

    strcpy(process.name, name);

    pid_t pid = __sync_fetch_and_add(&processes.counter, 1);
    __sync_add_and_fetch(&process_count, 1);

    if (pinfo != NULL) {
        rcparray_add(process.fiddies, pinfo->stdin);
        rcparray_add(process.fiddies, pinfo->stdout);
        rcparray_add(process.fiddies, pinfo->stderr);
 
        if (pinfo->cwd != NULL) {
            int len = strlen(pinfo->cwd) + 1;
            process.cwd = kmalloc(len);

            strcpy(process.cwd, pinfo->cwd);
        }
    }
    rcptable_set(processes, pid, process);
    return pid;
}

static char* generate_name(char* input) {
    input += strlen(input);
    while (*input != '/')
        --input;

    input++;

    char* end = strchrnul(input, '.');
    int len = end - input;

    char* ret = kmalloc(len + 1);
    memcpy(ret, input, len);
    ret[len] = '\0';

    return ret;
}

pid_t task_pricreate(char* image_path, char* args[], pcreate_info_t* pinfo) {
    finfo_t finfo;

    int ret = fs_finfo(image_path, &finfo);
    RETURN_IF_ERROR(ret);

    if (finfo.type == FS_TYPE_DIR)
        return -EISDIR;
    else if (finfo.type != FS_TYPE_REG)
        return -EINVAL;

    if (pinfo != NULL) {
        if (pinfo->cwd != NULL)
            RETURN_IF_ERROR(cwd_isvalid(pinfo->cwd));
    }

    char* name = generate_name(image_path);

    struct exec_data exec;
    pagemap_t* pgmap = NULL;

    ret = exec_load(image_path, args, &exec, &pgmap);
    RETURN_IF_ERROR(ret);
    
    pid_t new_pid  = task_prcreate(name, image_path, pgmap, pinfo);
    RETURN_IF_ERROR(new_pid);
    
    tid_t main_tid = task_tcreate(new_pid, exec.kernel_entry, false);

    spinlock_acquire(&sheduler_lock);
    init_entry_caller(thread_get(main_tid), exec.entry, 
                exec.init_code_addr, args_count(args));
    spinlock_release(&sheduler_lock);

    return new_pid;
}

void task_prstart(pid_t pid) {
    process_t* process = rcptable_get(processes, pid);
    if (process == NULL)
        return;

    int  lid;
    tid_t* id;

    rcparray_foreach(process->threads, lid) {
        id = rcparray_get(process->threads, lid);

        //printk("task: Starting pid %i\n", *id);
        task_tstart(*id);

        rcparray_unref(process->threads, lid);
    }

    rcptable_unref(processes, pid);
}

struct prkill_args {
    pid_t pid;
    bool fill_info;

    int code;

    volatile bool done;
};
typedef struct prkill_args prkill_args_t;

static void* _task_prkill_async(void* args_v) {
    prkill_args_t* args = args_v;
    //printk("task: pid %i kill attempt\n", args->pid);

    process_t* process = rcptable_get(processes, args->pid);
    if (process == NULL)
        return args_v;

    rcptable_remove(processes, args->pid);

    tid_t lid;
    tid_t* id;
    
    rcparray_foreach(process->threads, lid) {
        id = rcparray_get(process->threads, lid);

        //printk("pausing %i\n", *id);
        _task_prepare_tkill(*id);

        rcparray_unref(process->threads, lid);
    }

    rcparray_foreach(process->threads, lid) {
        id = rcparray_get(process->threads, lid);

        //printk("killing %i\n", *id);
        task_tkill(*id);

        rcparray_unref(process->threads, lid);
    }

    while (__sync_fetch_and_add(&process->busy, 0) > 0)
        task_tyield();

    process->ecode = args->code;

    pkill_hook_data_t data = {0};
    data.pid = args->pid;
    data.pstruct = process;
    
    hook_invoke(HOOK_PKILL, &data);

    event_defunct(&process->signal_event);
    while (rcptable_refcount(processes, args->pid) > 1)
        task_tyield();

    void* virt;
    phys_addr phys;
    uint32_t flags;

    kpforeach(virt, phys, flags, process->pagemap) {
        if (flags & PAGE_NOPHYS)
            continue;

        kffree(kfindexof(phys));
    }
    kp_dispose_map(process->pagemap);
    
    rcparray_dispose(process->fiddies);
    rcparray_dispose(process->threads);

    rcptable_unref(processes, args->pid);

    __sync_sub_and_fetch(&process_count, 1);
    return args_v;
}

static void _task_prkill_callback(void* ret_v) {
    prkill_args_t* args = ret_v;

    if (args->fill_info)
        args->done = true;

    kfree(args);
}

void task_prkill(pid_t pid, int code) {
    if (pid == KERNEL_PROCESS)
        return;

    volatile bool done = false;

    prkill_args_t* args = kzmalloc(sizeof(prkill_args_t));
    args->done = done;
    args->pid  = pid;
    args->code = code;

    args->fill_info = (pid != CURRENT_PID);

    task_call_async(_task_prkill_async, args, _task_prkill_callback);

    if (pid == CURRENT_PID)
        return;

    while (!args->done)
        task_tyield();
}

pagemap_t* task_prget_pmap(pid_t pid) {
    process_t* process = rcptable_get(processes, pid);
    if (process == NULL)
        return NULL;

    pagemap_t* pgmap = process->pagemap;
    rcptable_unref(processes, pid);

    return pgmap;
}

int task_prinfo(pid_t pid, process_info_t* info) {
    process_t* process = rcptable_get(processes, pid);
    if (process == NULL)
        return -ENOPS;

    memset(info, '\0', sizeof(process_info_t));
    info->used_phys_memory = (process->pagemap->frames_used 
                        + process->pagemap->dir_frames_used) * CPU_PAGE_SIZE;
    info->mapped_memory  = process->pagemap->map_frames_used * CPU_PAGE_SIZE;
    info->dir_phys_usage = process->pagemap->dir_frames_used * CPU_PAGE_SIZE;
    info->parent = process->parent;

    rcptable_unref(processes, pid);
    return 0;
}

bool task_pruse(pid_t pid) {
    process_t* process = rcptable_get(processes, pid);
    if (process == NULL)
        return false;

    __sync_add_and_fetch(&process->busy, 1);
    rcptable_unref(processes, pid);

    return true;
}

void task_prrelease(pid_t pid) {
    process_t* process = rcptable_get(processes, pid);
    if (process == NULL)
        return;

    __sync_sub_and_fetch(&process->busy, 1);
    rcptable_unref(processes, pid);
}

int task_prwait(pid_t pid, int* ecode) {
    process_t* process = rcptable_get(processes, pid);
    if (process == NULL)
        return -ENOPS;

    int ret = 0;
    if (event_wait(&process->signal_event))
        ret = -EINTR;

    *ecode = process->ecode;

    rcptable_unref(processes, pid);
    return ret;
}

int task_prset_cwd(pid_t pid, char* dir) {
    int len = strlen(dir);
    if (dir[len - 1] != '/')
        len++;

    RETURN_IF_ERROR(cwd_isvalid(dir));

    // check for perms
    
    process_t* process = rcptable_get(processes, pid);
    if (process == NULL)
        return -ENOPS;

    process->cwd = kmalloc(len + 1);
    strcpy(process->cwd, dir);

    process->cwd[len - 1] = '/';
    process->cwd[len]    = '\0';

    rcptable_unref(processes, pid);
    return 0;
}

int task_prget_cwd(pid_t pid, char* buffer, size_t size) {
    process_t* process = rcptable_get(processes, pid);
    if (process == NULL)
        return -ENOPS;

    if ((strlen(process->cwd) + 1) > size) {
        rcptable_unref(processes, pid);
        return -ERANGE;
    }

    strcpy(buffer, process->cwd);

    rcptable_unref(processes, pid);
    return 0;
}

void task_reshed_disable() {
    if (!sheduler_ready)
        return;

    __sync_add_and_fetch(&CURRENT_CPU_LOCAL.reshed_block, 1);
}

void task_reshed_enable() {
    if (!sheduler_ready)
        return;
        
    int busy = __sync_sub_and_fetch(&CURRENT_CPU_LOCAL.reshed_block, 1);

    if (__sync_fetch_and_add(&CURRENT_CPU_LOCAL.should_reshed, 0) == true
        && busy == 0)
        task_tyield();
}


void task_shed_choose() {
    __label__ done;

    if (__sync_fetch_and_add(&CURRENT_CPU_LOCAL.reshed_block, 0) > 0) {
        __sync_bool_compare_and_swap(&CURRENT_CPU_LOCAL.should_reshed, 
                                false, true);
        return;
    }
        
    if (!spinlock_try(&sheduler_lock))
        return;

    __sync_bool_compare_and_swap(&CURRENT_CPU_LOCAL.should_reshed, true, false);

    tid_t selected_tid = 0;
    tid_t tid = CURRENT_CPU_LOCAL.last_tid;
    size_t ms = time_get_ms_passed();

    thread_t* thread = thread_get(CURRENT_CPU_LOCAL.current_tid);
    if (thread != NULL) {
        thread->saved_stack = CURRENT_CPU_LOCAL.user_stack;
        thread_release(thread);
    }

    for (int i = 0; i <= thread_count; i++) {
        ++tid;

        while ((thread = thread_get(tid)) == NULL 
            && tid <= thread_count)
            tid++;

        if (tid > thread_count) {
            tid = 0;
            continue;
        }

        switch (thread->status) {
            case THREAD_STATUS_RUNNABLE:
                selected_tid = tid;

                if (!thread_try_take(thread))
                    continue;

                goto done;
            case THREAD_STATUS_SLEEPING:
                if (thread->resume_after >= ms)
                    break;

                selected_tid   = tid;
                thread->status = THREAD_STATUS_RUNNABLE;

                if (!thread_try_take(thread))
                    continue;

                goto done;
            default:
                break;
        }
    }

done:
    if (selected_tid == 0)
        thread = &idle_threads[CURRENT_CPU];
    else
        CURRENT_CPU_LOCAL.last_tid = selected_tid;

    CURRENT_CPU_LOCAL.current_tid = selected_tid;
    CURRENT_CPU_LOCAL.current_pid = thread->process;
    CURRENT_CPU_LOCAL.current_context = &(thread->context);
    CURRENT_CPU_LOCAL.kernel_stack = thread->kernel_stack;
    CURRENT_CPU_LOCAL.user_stack   = thread->saved_stack;

    task_set_kernel_stack(thread);
    spinlock_release(&sheduler_lock);
}

static void task_idle() {
    while (true)
        waitforinterrupt();
}

static void task_create_idle_thread(int i) {
    idle_threads[i].status       = THREAD_STATUS_RUNNABLE;
    idle_threads[i].kernel_stack = kzmalloc(256) + 256;
    idle_threads[i].kernelmode   = true;
    idle_threads[i].process      = 0;

    cpu_fill_context(&idle_threads[i].context, true, 
            task_idle, kernel_pmap.root_dir);

    cpu_set_stack(&idle_threads[i].context, kzmalloc(256), 256);
}

void task_syscall_init();
void cpu_set_local(int id, task_cpu_local_t* ptr);

void task_init() {
    rcptable_init(processes, 32, 1);

    pid_t pid = task_prcreate("aexkrnl", "/sys/aexkrnl.elf", &kernel_pmap, NULL);

    idle_threads    = kzmalloc(sizeof(thread_t) * 4);
    task_cpu_locals = kzmalloc(sizeof(task_cpu_local_t) * 4);
    
    thread_add(&idle_threads[0]);
    
    for (int i = 0; i < 4; i++) {
        task_cpu_locals[i].cpu_id = i;
        task_cpu_locals[i].current_pid = -1;
        task_cpu_locals[i].current_tid = -1;

        task_create_idle_thread(i);
    }
    tid_t tid = task_tcreate(pid, NULL, true);
    task_tstart(tid);

    thread_t* thread = thread_get(tid);
    
    CURRENT_CPU_LOCAL.current_pid = pid;
    CURRENT_CPU_LOCAL.current_tid = tid;
    CURRENT_CPU_LOCAL.current_context = &thread->context;
    CURRENT_CPU_LOCAL.last_tid = tid;
    CURRENT_CPU_LOCAL.kernel_stack = thread->kernel_stack;

    sheduler_ready = true;

    cpu_set_local(0, &task_cpu_locals[0]);

    task_call_async(_task_cleaner, NULL, NULL);
    task_syscall_init();
}