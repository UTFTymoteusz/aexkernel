#pragma once

#include "kernel/syscall.c"

void* syscalls[SYSCALL_AMOUNT];

void syscall_init();

// Handles a syscall
//void syscall_handler(int a, int b, int c, int d, int e, int f);