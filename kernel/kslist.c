#include "aex/mem.h"
#include "aex/string.h"

#include "aex/kernel.h"

#include "aex/kslist.h"

static uint32_t kslist_hash(char* string) {
    uint32_t ret = 0;
    while (*string++ != '\0') {
        ret += *string;
        ret += *string << 6;
        ret += (*string % 11) * 100;
        ret *= 3;
        ret = ((ret << 11) | (ret >> (32 - 11)));
    }
    return ret;
}

bool kslist_init(kslist_t* kslist) {
    kslist->count = 0;
    kslist->first = NULL;

    return true;
}

bool kslist_set(kslist_t* kslist, char* name, void* ptr) {
    kslist_entry_t* entry = kslist->first;
    kslist_entry_t* prev  = NULL;
    uint32_t hash = kslist_hash(name);

    while (entry != NULL) {
        if (entry->hash == hash) {
            int name_len = strlen(entry->name);
            if (memcmp(entry->name, name, name_len) == 0) {
                if (ptr == NULL) {
                    if (prev == NULL)
                        kslist->first = entry->next;
                    else
                        prev->next = entry->next;
                    
                    kfree(entry);
                    return true;
                }
                entry->data = ptr;
                return true;
            }
        }
        entry = entry->next;
    }

    if (ptr == NULL)
        return false;

    entry = kmalloc(sizeof(kslist_entry_t));

    if (kslist->first == NULL)
        kslist->first = entry;
    else
        prev->next = entry;

    entry->next = NULL;
    entry->hash = hash;
    entry->data = ptr;

    unsigned int name_len = strlen(entry->name);
    if (name_len > sizeof(entry->name) - 1)
        name_len = 31;

    memcpy(entry->name, name, name_len);
    entry->name[name_len] = '\0';

    return false;
}

void* kslist_get(kslist_t* kslist, char* name) {
    kslist_entry_t* entry = kslist->first;
    uint32_t hash = kslist_hash(name);

    while (entry != NULL) {
        if (entry->hash == hash) {
            int name_len = strlen(entry->name);
            if (memcmp(entry->name, name, name_len) == 0)
                return entry->data;
        }
        entry = entry->next;
    }
    return NULL;
}

void* kslist_iter(kslist_t* kslist, kslist_entry_t** entry) {
    if (*entry == NULL)
        *entry = kslist->first;
    else
        *entry = (*entry)->next;

    if (*entry == NULL)
        return NULL;

    return (*entry)->data;
}