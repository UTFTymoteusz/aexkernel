#pragma once

#include "aex/klist.h"
#include "dev/dev.h"

struct dev_incr_entry {
    char* pattern;
    char  c;
};
typedef struct dev_incr_entry dev_incr_t;

struct klist dev_incrementations;

int dev_name2id(char* name) {
    for (size_t i = 0; i < DEV_ARRAY_SIZE; i++) {
        if (dev_array[i] == NULL)
            continue;

        if (!strcmp(dev_array[i]->name, name))
            return i;
    }
    return DEV_ERR_NOT_FOUND;
}

int dev_id2name(int id, char* buffer) {
    if (dev_array[id] == NULL)
        return DEV_ERR_NOT_FOUND;

    memcpy(buffer, dev_array[id]->name, strlen(dev_array[id]->name) + 1);
    return 0;
}

char* dev_name_inc(char* pattern, char* buffer) {
    klist_entry_t* klist_entry = NULL;
    dev_incr_t* entry = NULL;

    while (true) {
        entry = (dev_incr_t*)klist_iter(&dev_incrementations, &klist_entry);

        if (entry == NULL)
            break;

        if (!strcmp(entry->pattern, pattern))
            break;
    }
    if (entry == NULL) {
        entry = kmalloc(sizeof(dev_incr_t));

        klist_set(&dev_incrementations, dev_incrementations.count + 1, (void*)entry);

        entry->pattern = kmalloc(8);
        entry->c = 'a';

        memcpy(entry->pattern, pattern, 8);
    }
    else
        entry->c++;

    int i = 0;
    while (pattern[i] != '\0') {
        switch (pattern[i]) {
            case '@':
                buffer[i] = entry->c;
                break;
            default:
                buffer[i] = pattern[i];
                break;
        }
        ++i;
    }
    buffer[i] = '\0';
    return buffer;
}