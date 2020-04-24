// rcptable - Reference counting pointer table

// basically same idea as rcparray, but has custom cleanup func, uses hash
// tables, and can do indexes freely

// 1st - hash segments
// 2nd - struct pointer arrays
// 3rd - struct pointer

#include "aex/mem.h"
#include "aex/spinlock.h"

#define rcptable(type) \
    struct { \
        struct {   \
            size_t id;      \
            int  refs;    \
            bool present; \
            type value;   \
        } *** table; \
        \
        int*   sizes;     \
        size_t counter;   \
        size_t seg_count; \
        void (*cleanup)(type* value); \
        spinlock_t lock; \
    }

#define rcptable_init(rcptable, hashsegs, _counter) \
    rcptable.seg_count = hashsegs; \
    rcptable.counter   = _counter;  \
    \
    rcptable.sizes = kzmalloc(sizeof(int) * hashsegs);   \
    rcptable.table = kzmalloc(sizeof(void*) * hashsegs); \
    \
    rcptable.cleanup = NULL; \
    rcptable.lock.val = 0;

#define rcptable_cinit(rcptable, hashsegs, _counter, _cleanup) \
    rcptable.seg_count = hashsegs; \
    rcptable.counter   = _counter;  \
    \
    rcptable.sizes = kzmalloc(sizeof(int) * hashsegs);   \
    rcptable.table = kzmalloc(sizeof(void*) * hashsegs); \
    \
    rcptable.cleanup = _cleanup; \
    rcptable.lock.val = 0;

#define rcptable_hash(id, size) ((size_t) id % (size_t) size)

#define rcptable_set(rcptable, _id, val) ({ \
    size_t __id = (size_t) _id; \
    int i; \
    \
    spinlock_acquire(&rcptable.lock); \
    \
    size_t hash = rcptable_hash(__id, rcptable.seg_count); \
    \
    for (i = 0; i < rcptable.sizes[hash]; i++) { \
        if (rcptable.table[hash][i] == NULL)     \
            break; \
    } \
      \
    if (i >= rcptable.sizes[hash]) \
        rcptable.sizes[hash] += i - rcptable.sizes[hash] + 1; \
    \
    rcptable.table[hash] = \
        krealloc(rcptable.table[hash], \
            rcptable.sizes[hash] * sizeof(**(rcptable.table))); \
    \
    rcptable.table[hash][i] = kmalloc(sizeof(***(rcptable.table))); \
    rcptable.table[hash][i]->id    = __id;   \
    rcptable.table[hash][i]->refs  = 1;      \
    rcptable.table[hash][i]->value = val;    \
    rcptable.table[hash][i]->present = true; \
    \
    spinlock_release(&rcptable.lock); \
})

#define rcptable_get(rcptable, _id) ({ \
    __label__ done;  \
    __label__ found; \
    \
    size_t __id = (size_t) _id; \
    int i; \
           \
    void* ret = NULL; \
    \
    spinlock_acquire(&rcptable.lock); \
    \
    size_t hash = rcptable_hash(__id, rcptable.seg_count); \
    \
    for (i = 0; i < rcptable.sizes[hash]; i++) { \
        if (rcptable.table[hash][i] != NULL      \
         && rcptable.table[hash][i]->id == __id)  \
            goto found; \
    } \
    goto done; \
    \
found: \
    if (!rcptable.table[hash][i]->present) \
        goto done; \
    \
    rcptable.table[hash][i]->refs++; \
    ret = &rcptable.table[hash][i]->value; \
    \
done: \
    spinlock_release(&rcptable.lock); \
    ret;\
})

#define rcptable_unref(rcptable, _id) ({ \
    __label__ done;  \
    __label__ found; \
    \
    size_t __id = (size_t) _id; \
    int i; \
    \
    spinlock_acquire(&rcptable.lock); \
    \
    size_t hash = rcptable_hash(__id, rcptable.seg_count); \
    \
    for (i = 0; i < rcptable.sizes[hash]; i++) { \
        if (rcptable.table[hash][i] != NULL      \
         && rcptable.table[hash][i]->id == __id)  \
            goto found; \
    } \
    goto done; \
       \
found: \
    rcptable.table[hash][i]->refs--; \
    if (rcptable.table[hash][i]->refs == 0) { \
        if (rcptable.cleanup != NULL) {       \
            rcptable.cleanup(&rcptable.table[hash][i]->value); \
            \
            kfree(rcptable.table[hash][i]); \
            rcptable.table[hash][i] = NULL; \
            goto done; \
        } \
        kfree(rcptable.table[hash][i]); \
        rcptable.table[hash][i] = NULL; \
    } \
      \
done: \
    spinlock_release(&rcptable.lock); \
})

#define rcptable_remove(rcptable, _id) ({ \
    __label__ done;  \
    __label__ found; \
    \
    size_t __id = (size_t) _id; \
    int i; \
    \
    spinlock_acquire(&rcptable.lock); \
    \
    size_t hash = rcptable_hash(__id, rcptable.seg_count); \
    \
    for (i = 0; i < rcptable.sizes[hash]; i++) { \
        if (rcptable.table[hash][i] != NULL      \
         && rcptable.table[hash][i]->id == __id)  \
            goto found; \
    } \
    goto done; \
    \
found: \
    rcptable.table[hash][i]->present = false; \
    rcptable.table[hash][i]->refs--; \
    \
    if (rcptable.table[hash][i]->refs == 0) { \
        if (rcptable.cleanup != NULL) {       \
            rcptable.cleanup(&rcptable.table[hash][i]->value); \
            \
            kfree(rcptable.table[hash][i]); \
            rcptable.table[hash][i] = NULL; \
            goto done; \
        } \
        kfree(rcptable.table[hash][i]); \
        rcptable.table[hash][i] = NULL; \
    } \
      \
done: \
    spinlock_release(&rcptable.lock); \
})

#define rcptable_refcount(rcptable, _id) ({ \
    __label__ done;  \
    __label__ found; \
    \
    size_t __id = (size_t) _id; \
    int i; \
           \
    int ret = -1; \
    \
    spinlock_acquire(&rcptable.lock); \
    \
    size_t hash = rcptable_hash(__id, rcptable.seg_count); \
    \
    for (i = 0; i < rcptable.sizes[hash]; i++) { \
        if (rcptable.table[hash][i] != NULL      \
         && rcptable.table[hash][i]->id == __id)  \
            goto found; \
    } \
    goto done; \
       \
found: \
    ret = rcptable.table[hash][i]->refs; \
    \
done: \
    spinlock_release(&rcptable.lock); \
    ret;\
})

#define rcptable_dispose(rcptable) \
    ({  \
        spinlock_acquire(&rcptable.lock); \
        \
        for (size_t hs = 0; hs < rcptable.seg_count; hs++) { \
            if (rcptable.table[hs] == NULL) \
                continue; \
            \
            for (size_t id = 0; id < rcptable.sizes[hs]; id++) { \
                if (rcptable.table[hs][id] == NULL) \
                    continue; \
                \
                kfree(rcptable.table[hs][id]); \
            } \
            kfree(rcptable.table[hs]); \
        } \
        spinlock_release(&rcptable.lock); \
    })
