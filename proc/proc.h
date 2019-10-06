#pragma once

struct thread {
    size_t id;

    char* name;;

    struct process* parent;
    struct task_descriptor* task;
};

struct process {
    size_t pid;

    char* name;
    char* image_path;

    struct klist threads, fiddies;

    size_t thread_counter;
    size_t fiddie_counter;

    size_t paging_dir;
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