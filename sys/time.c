#include "aex/proc/task.h"
#include "aex/sys/time.h"

double tick_interval = 0;
size_t ticks_passed = 0;

double time_get_ms_passed() {
    return ticks_passed * tick_interval;
}

void time_set_tick_interval(double interval) {
    tick_interval = interval;
}

void time_tick() {
    ticks_passed++;
}