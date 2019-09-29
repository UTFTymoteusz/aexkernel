#pragma once

#include "dev.h"

int dev_name2id(char* name) {

    for (size_t i = 0; i < DEV_COUNT; i++) {

        if (dev_list[i] == NULL)
            continue;

        if (dev_list[i]->name == name)
            return i;
    }
    return -1;
}
int dev_id2name(int id, char* buffer) {
    
    if (dev_list[id] == NULL)
        return -1;

    memcpy(buffer, dev_list[id]->name, strlen(dev_list[id]->name) + 1);
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