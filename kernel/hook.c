#include "aex/kmem.h"

#include <stddef.h>
#include <string.h>

#include "hook.h"

int _hook_pstart = 0;
int _hook_pkill  = 1;
int _hook_usr_faccess = 2;

struct hook_func {
    char name[48];
    void* (*func)(void* data); // va_list here would be too messy

    struct hook_func* next;
};
typedef struct hook_func hook_func_t;

hook_func_t* hooks[MAX_HOOKS];

void hook_add(int id, char* name, void* func) {
    if (id >= MAX_HOOKS || id < 0)
        return;

    hook_func_t* hook = hooks[id];
    hook_func_t* prev = NULL;

    int nlen = strlen(name);

    if (hook == NULL) {
        hooks[id] = kmalloc(sizeof(hook_func_t));
        hook = hooks[id];

        memset(hook, 0, sizeof(hook_func_t));
        hook->func = func;

        strcpy(hook->name, name);
        return;
    }

    while (hook != NULL) {
        prev = hook;
        hook = hook->next;
    }
    prev->next = kmalloc(sizeof(hook_func_t));
    hook = prev->next;

    memset(hook, 0, sizeof(hook_func_t));
    hook->func = func;

    strcpy(hook->name, name);
}

void hook_remove(int id, char* name) {
    if (id >= MAX_HOOKS || id < 0)
        return;

    if (hooks[id] == NULL)
        return;

    hook_func_t* hook = hooks[id];
    hook_func_t* prev = NULL;

    while (hook != NULL) {
        if (strcmp(hook->name, name) == 0) {
            if (prev == NULL) {
                hooks[id] = hook->next;
                break;
            }
            prev->next = hook->next;
            break;
        }
        prev = hook;
        hook = hook->next;
    }
}

void hook_invoke(int id, void* data) {
    if (id >= MAX_HOOKS || id < 0)
        return;

    if (hooks[id] == NULL)
        return;

    hook_func_t* hook = hooks[id];

    while (hook != NULL) {
        hook->func(data);
        hook = hook->next;
    }
}

void hook_first(int id, void** pointer) {
    if (id >= MAX_HOOKS || id < 0)
        *pointer = NULL;

    *pointer = hooks[id];
}

void* hook_invoke_advance(int id, void* data, void** pointer) {
    if (id >= MAX_HOOKS || id < 0)
        return NULL;

    if (hooks[id] == NULL)
        return NULL;

    if (*pointer == NULL)
        *pointer = hooks[id];
    
    hook_func_t* hook = *pointer;
    *pointer = hook->next;

    return hook->func(data);
}