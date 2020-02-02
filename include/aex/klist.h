#pragma once

#include "stdbool.h"
#include "stddef.h"
#include "stdint.h"

struct klist {
    size_t count;
    struct klist_entry* first;
};
typedef struct klist klist_t;

struct klist_entry {
    size_t index;
    void* data;

    struct klist_entry* next;
};
typedef struct klist_entry klist_entry_t;

#define klist_foreach(klist, var, entry) entry = NULL; for (; (var = klist_iter(klist, &entry)) != NULL; )

bool  klist_init(struct klist* klist);
void  klist_init_raw(struct klist* klist, klist_entry_t* first_entry);
bool  klist_set(struct klist* klist, size_t index, void* ptr);
void* klist_get(struct klist* klist, size_t index);

int64_t klist_first(struct klist* klist);
void*   klist_iter(struct klist* klist, klist_entry_t** entry);