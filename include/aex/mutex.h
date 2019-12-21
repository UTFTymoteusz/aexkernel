#pragma once

#include <stdbool.h>

typedef volatile int mutex_t;

void mutex_acquire(mutex_t*);
void mutex_acquire_yield(mutex_t*);
void mutex_release(mutex_t*);

bool mutex_try(mutex_t*);

void mutex_wait(mutex_t*);
void mutex_wait_yield(mutex_t*);