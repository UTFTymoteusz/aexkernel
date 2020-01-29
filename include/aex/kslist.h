#pragma once

#include "stdbool.h"
#include "stddef.h"
#include "stdint.h"

struct kslist {
    size_t count;
    struct kslist_entry* first;
};
typedef struct kslist kslist_t;

struct kslist_entry {
    uint32_t hash;
    char name[32];

    void* data;

    struct kslist_entry* next;
};
typedef struct kslist_entry kslist_entry_t;

bool  kslist_init(kslist_t* kslist);
bool  kslist_set(kslist_t* kslist, char* key, void* ptr);
void* kslist_get(kslist_t* kslist, char* key);

void* kslist_iter(kslist_t* kslist, kslist_entry_t** entry);