#include "aex/kslist.h"
#include "aex/mem.h"
#include "aex/rcode.h"
#include "aex/string.h"

#include "aex/proc/exec.h"

kslist_t* exec_executors;

int exec_load(char* path, char* args[], struct exec_data* exec, paging_descriptor_t* proot) {
    kslist_entry_t* kslist_entry = NULL;
    int (*executor)(char* path, char* args[], struct exec_data* exec, paging_descriptor_t* proot);

    int ret = 0;

    while (true) {
        executor = kslist_iter(exec_executors, &kslist_entry);
        if (executor == NULL)
            break;

        ret = executor(path, args, exec, proot);
        IF_ERROR(ret)
            continue;

        return ret;
    }
    return ERR_INVALID_EXE;
}

int exec_register_executor(char* name, int (*executor_function)(char* path, char* args[], struct exec_data* exec, paging_descriptor_t* proot)) {
    if (exec_executors == NULL) {
        exec_executors = kmalloc(sizeof(kslist_t));
        kslist_init(exec_executors);
    }
    kslist_set(exec_executors, name, executor_function);
    
    return 0;
}

int exec_unregister_executor(char* name) {
    return kslist_set(exec_executors, name, NULL) ? 0 : -1;
}