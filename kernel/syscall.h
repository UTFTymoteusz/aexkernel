#pragma once

#include "kernel/syscall.c"

struct syscall_args;


void* syscall_handlers[SYSCALL_AMOUNT];

void syscall_init();

// Handles a syscall
void syscall_handler(struct syscall_args* args);