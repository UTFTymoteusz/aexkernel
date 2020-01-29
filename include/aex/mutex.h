#pragma once

#include <stdbool.h>

struct mutex {
    volatile int val;
    char* name;
};
typedef struct mutex mutex_t;

void mutex_init(mutex_t*);

void mutex_acquire(mutex_t*);
void mutex_acquire_raw(mutex_t*);
void mutex_acquire_yield(mutex_t*);
void mutex_release(mutex_t*);
void mutex_release_raw(mutex_t*);

bool mutex_try(mutex_t*);
bool mutex_try_raw(mutex_t*);

void mutex_wait(mutex_t*);
void mutex_wait_yield(mutex_t*);