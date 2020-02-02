#pragma once

#include <stdbool.h>

#define create_spinlock() {.val = 0, .name = NULL}

struct spinlock {
    volatile int val;
    char* name;
};
typedef struct spinlock spinlock_t;

void spinlock_init(spinlock_t*);

void spinlock_acquire(spinlock_t*);
void spinlock_release(spinlock_t*);

bool spinlock_try(spinlock_t*);

void spinlock_wait(spinlock_t*);