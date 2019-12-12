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
#define SYSCALL_FINFO    10
#define SYSCALL_FCOUNT   11
#define SYSCALL_FLIST    12
#define SYSCALL_IOCTL    13

#define SYSCALL_PGALLOC  14
#define SYSCALL_PGFREE   15

#define SYSCALL_SPAWN    16
#define SYSCALL_EXEC     17
#define SYSCALL_WAIT     18
#define SYSCALL_GETPID   19
#define SYSCALL_GETTHID  20

#define SYSCALL_GETCWD   21
#define SYSCALL_SETCWD   22

#define SYSCALL_PROCTEST 77

#define SYSCALL_AMOUNT 128

void* syscalls[SYSCALL_AMOUNT];