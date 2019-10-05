#pragma once

#include "aex/klist.c"

struct klist;
struct klist_entry;

typedef struct klist_entry klist_entry_t;

bool  klist_init(struct klist* klist);
bool  klist_set(struct klist* klist, size_t index, void* ptr);
void* klist_get(struct klist* klist, size_t index);

size_t klist_first(struct klist* klist);
void*  klist_iter(struct klist* klist, struct klist_entry** entry);