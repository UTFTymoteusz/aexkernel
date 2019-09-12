#pragma once

#include "kernel/syscall.c"

void* syscalls[SYSCALL_AMOUNT];

void syscall_init();