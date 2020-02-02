#pragma once

void debug_stacktrace();
void debug_print_registers();

void debug_load_symbols();
char* debug_resolve_symbol(void* addr);