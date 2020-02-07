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

#define rcparray_foreach(rcparray, id, val, done) \
    for (({spinlock_acquire(&(rcparray.lock)); id = 0;}); ({ \
        bool a = false; \
        \
        if (done) \
            id = rcparray.size + 2; \
        \
        for (; id < rcparray.size; id++) { \
            if (rcparray.array[id]->present) {   \
                val = &rcparray.array[id]->value; \
                a = true; \
                break; \
            }   \
        }       \
                \
        if (!a) \
            spinlock_release(&(rcparray.lock)); \
        \
        a; \
    }); id++)