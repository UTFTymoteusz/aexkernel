#include "aex/mem.h"
#include "aex/klist.h"
#include "aex/rcode.h"
#include "aex/string.h"

#include "aex/dev/dev.h"

#include "aex/dev/name.h"

struct dev_incr_entry {
    char* pattern;
    char  c;
};
typedef struct dev_incr_entry dev_incr_t;

extern rcparray(dev_t*) dev_array;

struct klist dev_incrementations;

int dev_name2id(char* name) {
    int id = -1;

    rcparray_find(dev_array, dev_t*, strcmp((*elem)->name, name) == 0, &id);
    if (id == -1)
        return -ENODEV;

    rcparray_unref(dev_array, id);
    return id;
}

int dev_id2name(int id, char buffer[32]) {
    dev_t** _dev = rcparray_get(dev_array, id);
    if (_dev == NULL)
        return -1;

    memcpy(buffer, (*_dev)->name, 32);
    rcparray_unref(dev_array, id);

    return 0;
}

char* dev_name_inc(char* pattern, char* buffer) {
    klist_entry_t* klist_entry = NULL;
    dev_incr_t* entry = NULL;

    while (true) {
        entry = (dev_incr_t*) klist_iter(&dev_incrementations, &klist_entry);

        if (entry == NULL)
            break;

        if (!strcmp(entry->pattern, pattern))
            break;
    }
    if (entry == NULL) {
        entry = kmalloc(sizeof(dev_incr_t));

        klist_set(&dev_incrementations, dev_incrementations.count + 1, (void*) entry);

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