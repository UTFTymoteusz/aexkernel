#pragma once

#include <stdint.h>

void (*shutdown_func)(void);

void register_shutdown(void* func);
void shutdown();

void sys_funckey(uint8_t key);
void sys_sysrq(uint8_t key);

void sys_init();