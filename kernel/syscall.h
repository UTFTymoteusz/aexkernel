#pragma once

#define SYSCALL_SLEEP    0
#define SYSCALL_YIELD    1
#define SYSCALL_PROCTEST 2
#define SYSCALL_FOPEN    3
#define SYSCALL_FREAD    4
#define SYSCALL_FWRITE   5
#define SYSCALL_FCLOSE   6
#define SYSCALL_FSEEK    7
#define SYSCALL_PGALLOC  8
#define SYSCALL_PGFREE   9

#define SYSCALL_AMOUNT 128

void* syscalls[SYSCALL_AMOUNT];

void syscall_init();