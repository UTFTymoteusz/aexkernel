#include "aex/kmem.h"

#include "klist.h"

#include <stdio.h>

struct klist;
struct klist_entry;

typedef struct klist_entry klist_entry_t;

bool klist_init(struct klist* klist) {
    klist->count = 0;
    klist->first = NULL;

    return true;
}
void klist_init_raw(struct klist* klist, klist_entry_t* first_entry) {
    klist->count = 1;
    klist->first = first_entry;

    first_entry->next  = NULL;
}

bool klist_set(struct klist* klist, size_t index, void* ptr) {
    struct klist_entry* entry = klist->first;
    struct klist_entry* new   = NULL;
    struct klist_entry* prev  = NULL;

    if (entry == NULL) {
        if (ptr == NULL)
            return false;

        new = kmalloc(sizeof(struct klist_entry));

        new->index = index;
        new->data  = ptr;
        new->next  = NULL;

        klist->first = new;
        klist->count++;

        return false;
    }
    while (true) {
        if (entry->index == index) {
            if (ptr == NULL) {
                if (prev == NULL)
                    klist->first = entry->next;
                else
                    prev->next = entry->next;

                kfree((void*) entry);
                klist->count--;

                return true;
            }
            entry->data = ptr;
            return true;
        }
        prev = entry;

        if (entry->next == NULL)
            break;

        entry = entry->next;
    }
    if (ptr == NULL)
        return false;

    new = kmalloc(sizeof(struct klist_entry));

    new->index = index;
    new->data  = ptr;
    new->next  = NULL;

    entry->next = new;
    klist->count++;

    return false;
}

void* klist_get(struct klist* klist, size_t index) {
    struct klist_entry* entry = klist->first;

    while (entry != NULL) {
        if (entry->index == index)
            return entry->data;

        entry = entry->next;
    }
    return NULL;
}

int64_t klist_first(struct klist* klist) {
    if (klist->first != NULL)
        return klist->first->index;

    return -1;
}

void* klist_iter(struct klist* klist, klist_entry_t** entry) {
    if (*entry == NULL)
        *entry = klist->first;
    else
        *entry = (*entry)->next;

    if (*entry == NULL)
        return NULL;

    return (*entry)->data;
}