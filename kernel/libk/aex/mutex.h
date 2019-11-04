#pragma once

typedef volatile int mutex_t;

void mutex_acquire(mutex_t*);
void mutex_acquire_yield(mutex_t*);
void mutex_release(mutex_t*);