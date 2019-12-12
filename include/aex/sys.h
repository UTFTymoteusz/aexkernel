#pragma once

void (*shutdown_func)(void);

void kpanic(char* msg);

void register_shutdown(void* func);
void shutdown();