#pragma once

#include "kernel/debug.h"

#define SYSCALL_AMOUNT 32

void* syscalls[SYSCALL_AMOUNT];

void test() {
    printf("SYSCALLS BOII\n");
}

void syscall_init() {
    
}

/*void syscall_handler(int a, int b, int c, int d, int e, int f) {

    //write_debug("Syscall %s\n", args->sys_id, 10);
    write_debug(" A: 0x%s\n", a, 16);
    write_debug(" B: 0x%s\n", b, 16);
    write_debug(" C: 0x%s\n", c, 16);
    write_debug(" D: 0x%s\n", d, 16);
    write_debug(" E: 0x%s\n", e, 16);
    write_debug(" F: 0x%s\n", f, 16);

    printf("SYSCALLS BOII\n");
}*/