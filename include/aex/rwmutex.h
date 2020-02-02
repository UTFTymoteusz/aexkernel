#include <stdbool.h>

struct rwmutex {
    volatile int read;
    volatile int write;

    volatile bool removed;
};
typedef struct rwmutex rwmutex_t;

bool rwmutex_acquire_read (rwmutex_t* rwmutex);
bool rwmutex_acquire_write(rwmutex_t* rwmutex);

void rwmutex_release_read (rwmutex_t* rwmutex);
void rwmutex_release_write(rwmutex_t* rwmutex);

void rwmutex_signal_remove(rwmutex_t* rwmutex);