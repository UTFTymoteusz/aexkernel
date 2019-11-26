#pragma once

#define SYSCALL_SLEEP     0
#define SYSCALL_YIELD     1
#define SYSCALL_EXIT      2
#define SYSCALL_THEXIT    3
#define SYSCALL_FOPEN     4
#define SYSCALL_FREAD     5
#define SYSCALL_FWRITE    6
#define SYSCALL_FCLOSE    7
#define SYSCALL_FSEEK     8
#define SYSCALL_FEXISTS   9
#define SYSCALL_FINFO    17
#define SYSCALL_PGALLOC  10
#define SYSCALL_PGFREE   11
#define SYSCALL_SPAWN    12
#define SYSCALL_EXEC     13
#define SYSCALL_WAIT     14
#define SYSCALL_GETCWD   15
#define SYSCALL_CHDIR    16

#define SYSCALL_PROCTEST 23

#define SYSCALL_AMOUNT 128

void* syscalls[SYSCALL_AMOUNT];

void syscall_init();