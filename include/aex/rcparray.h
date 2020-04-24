// rcparray - Reference counting pointer array

#pragma once

#include "aex/mem.h"
#include "aex/spinlock.h"

#include "aex/kernel.h"

#define RCPARRAY_SLAB 8

#define rcparray(type) \
    struct { \
        struct {   \
            int  refs;    \
            bool present; \
            type value;   \
        } ** array; \
        \
        size_t     size; \
        spinlock_t lock; \
    }

#define rcparray_add(rcparray, data) \
    ({  \
        __label__ found_free; \
        \
        spinlock_acquire(&(rcparray.lock)); \
        \
        size_t size = rcparray.size; \
        size_t i    = 0; \
        \
        for (i = 0; i < size; i++) {   \
            if (rcparray.array[i] == NULL) { \
                rcparray.array[i] = kmalloc(sizeof(**(rcparray.array))); \
                goto found_free;  \
            } \
        }     \
        \
        rcparray.size++; \
        rcparray.array = krealloc(rcparray.array, \
                            sizeof(*(rcparray.array)) * rcparray.size); \
        \
        for (size_t j = i; j < rcparray.size; j++) \
            rcparray.array[j] = kmalloc(sizeof(**(rcparray.array))); \
        \
    found_free: \
        \
        rcparray.array[i]->value   = data; \
        rcparray.array[i]->present = true; \
        rcparray.array[i]->refs    = 1;    \
        \
        spinlock_release(&(rcparray.lock)); \
        i; \
    });

#define rcparray_get(rcparray, id) \
    ({ \
        __label__ done;     \
        void* value = NULL; \
        size_t _id = id;    \
        \
        spinlock_acquire(&(rcparray.lock)); \
        if (_id >= rcparray.size) \
            goto done; \
        \
        if (rcparray.array[_id] == NULL) \
            goto done; \
        \
        if (!(rcparray.array[_id]->present)) \
            goto done; \
        \
        value = &rcparray.array[_id]->value; \
        rcparray.array[_id]->refs++;        \
          \
    done: \
        spinlock_release(&(rcparray.lock)); \
        value; \
    })

#define rcparray_unref(rcparray, id) \
    ({ \
        __label__ done;     \
        size_t _id = id;    \
        \
        spinlock_acquire(&(rcparray.lock)); \
        if (_id >= rcparray.size || rcparray.array[_id] == NULL) \
            goto done; \
        \
        rcparray.array[_id]->refs--; \
        if (rcparray.array[_id]->refs == 0) { \
            kfree(rcparray.array[_id]); \
            rcparray.array[_id] = NULL; \
        } \
          \
    done: \
        spinlock_release(&(rcparray.lock)); \
    })

#define rcparray_remove(rcparray, id) \
    ({ \
        __label__ done;     \
        size_t _id = id;    \
        int refs = -1;      \
        \
        spinlock_acquire(&(rcparray.lock)); \
        if (_id >= rcparray.size || rcparray.array[_id] == NULL) \
            goto done; \
        \
        rcparray.array[_id]->present = false; \
        refs = --rcparray.array[_id]->refs;   \
        \
        if (rcparray.array[_id] != NULL && rcparray.array[_id]->refs == 0) { \
            kfree(rcparray.array[_id]); \
            rcparray.array[_id] = NULL; \
        } \
          \
    done: \
        spinlock_release(&(rcparray.lock)); \
        refs; \
    })

#define rcparray_foreach(rcparray, id) \
    for (id = 0; ({     \
        bool a = false; \
        size_t _id = (size_t) id; \
        \
        spinlock_acquire(&(rcparray.lock)); \
        \
        for (; _id < rcparray.size; _id++) {    \
            if (rcparray.array[_id] == NULL)    \
                continue; \
                          \
            if (rcparray.array[_id]->present) { \
                a = true; \
                break;    \
            }   \
        }       \
        spinlock_release(&(rcparray.lock)); \
           \
        a; \
    }); id++)

#define rcparray_find(rcparray, type, cond, id) \
    ({     \
        type* elem = NULL;    \
        \
        spinlock_acquire(&(rcparray.lock)); \
        \
        for (size_t _id = 0; _id < rcparray.size; _id++) {    \
            if (rcparray.array[_id] == NULL)    \
                continue; \
                          \
            if (rcparray.array[_id]->present) {     \
                elem = &rcparray.array[_id]->value; \
                if (!(cond)) {   \
                    elem = NULL; \
                    continue; \
                } \
                  \
                *id = _id; \
                rcparray.array[_id]->refs++; \
                break;     \
            }   \
        }       \
        spinlock_release(&(rcparray.lock)); \
              \
        elem; \
    })

#define rcparray_dispose(rcparray) \
    ({  \
        spinlock_acquire(&(rcparray.lock)); \
        \
        for (size_t id = 0; id < rcparray.size; id++) { \
            if (rcparray.array[id] == NULL) \
                continue; \
                          \
            kfree(rcparray.array[id]); \
        } \
        \
        kfree(rcparray.array); \
        spinlock_release(&(rcparray.lock)); \
    })
